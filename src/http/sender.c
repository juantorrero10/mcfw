#include <private/mcfw.h>


// NOTE: if asclib updates to multithreading, mark this as thread local
char g_cstr_buff[4096];


void sender_default(_Out_ HttpSender* str, int client_fd) {
    str->headers_sent = false;
    str->client_fd = client_fd;
    str->mime_flags = 0;
    str->encoding = TRANSFER_INMEDIATE;
    str->data.inm = DB_FROM(DS_EMPTY());
    str->mime_compression = MIME_ALGO_NONE;
}

void sender_destroy(HttpSender* str) {
    switch (str->encoding) {
        case TRANSFER_CHUNKED: {
            DBuffer* db = &str->data.chunked.buffer;
            if (db->is_heap_alloced && DH_IS_VALID(db->buff.hp)) {
                DH_FREE(db->buff.hp);
            }
            *db = DB_FROM(DS_EMPTY());
            break;
        }
        case TRANSFER_INMEDIATE: {
            DBuffer* db = &str->data.inm;
            if (db->is_heap_alloced && DH_IS_VALID(db->buff.hp)) {
                DH_FREE(db->buff.hp);
            }
            *db = DB_FROM(DS_EMPTY());
            break;
        }
        case TRANSFER_FILE: {
            if (str->data.file.file_fd > 0) {
                close(str->data.file.file_fd);
            }
            str->data.file.file_fd = -1;
            break;
        }
        default: break;
    }
}

void sender_set_mime_flags(HttpSender* str, HttpContentType t) {
    if (t == CONTENT_OTHER || t == CONTENT_NOT_SPECIFIED) {
        str->mime_compression = MIME_ALGO_NONE;
        str->mime_flags = 0;
        return;
    }
    MIMELutEntry* e = &MIME_LUT[t];
    str->mime_compression = e->comp_algorithm;
    str->mime_flags = e->flags;
}

void sender_configure_inm(HttpSender* str, DBuffer db, HttpContentType t) {
    str->encoding = TRANSFER_INMEDIATE;
    str->total_bytes = db.buff.hp.count;
    str->data.inm = db;
    sender_set_mime_flags(str, t);
}



void sender_configure_file(HttpSender* str, DStatic file_path, HttpContentType t, DStatic range_header) {
    str->encoding = TRANSFER_FILE;  
    str->data.file.file_fd = -1;
    str->data.file.file_name = file_path;


    struct stat fs;
    stat(make_cstr(file_path), &fs);

    // TODO: Range header parse and get its size
    (void)range_header; // Ignore header for now
    str->total_bytes = fs.st_size;
    str->data.file.last_modified = fs.st_mtime;
    sender_set_mime_flags(str, t);
}

#define ST_ERR_TIMEOUT (SendError){ .data.error_code = HTTP_GONE, .is_ok = false, .timeout = true}
#define ST_ERR(CODE) (SendError){ .data.error_code = CODE, .is_ok = false, .timeout = false }
#define ST_OK (SendError){ .data.vec = NULL, .is_ok = true }

SendError sender_push(HttpSender* str, uint64_t timeout) {

    if (str->total_bytes == 0) {
        return STREAMING_OK;
    }

    bool send_headers = !str->headers_sent &&
        DS_IS_VALID(str->resp_headers) &&
        str->resp_headers.count > 0;

    switch (str->encoding) {
        case TRANSFER_INMEDIATE: {
            struct iovec vecs[2];
            size_t n_vecs = 0;
            if (send_headers) {
                vecs[0] = (struct iovec){
                    .iov_base = str->resp_headers.s_ptr,
                    .iov_len = str->resp_headers.count, 
                };
                n_vecs++;
            }
            DBuffer* db = &str->data.inm; 
            if (db->buff.hp.h_ptr != NULL && db->buff.hp.count > 0) {
                vecs[n_vecs] = (struct iovec){
                    .iov_base = db->buff.hp.h_ptr,
                    .iov_len = db->buff.hp.count, 
                };
                n_vecs++;
            }
            struct msghdr full_msg = {
                .msg_iov = vecs, .msg_iovlen = n_vecs
            };
            
            // Async send
            ssize_t sz = io_timeout(ssize_t, asc_sendmsg(str->client_fd, &full_msg), timeout, {
                return STREAMING_TIMEOUT;
            });
            if (sz < 0) {
                mlog_warn("couldn't send to client: %s", strerror(errno));
                return STREAMING_IO_ERROR;
            }
            str->total_bytes -= sz;
            str->headers_sent = true;
            break;
        }
        case TRANSFER_FILE: {
            if (send_headers) {
                DStatic* h = &str->resp_headers;
                ssize_t sz = io_timeout(ssize_t, asc_send(str->client_fd, h->s_ptr, h->count), timeout,{
                    return STREAMING_TIMEOUT;
                });
                if (sz < 0) {
                    mlog_warn("couldn't send to client: %s", strerror(errno));
                    return STREAMING_IO_ERROR;
                }
                str->headers_sent = true;
            }
            // Send file contents
            int fd = asc_open(make_cstr(str->data.file.file_name), O_RDONLY, 0);
            if (fd < 0) {
                mlog_warn("Couldn't open fd of path %s: %s", g_cstr_buff, strerror(errno));
                return STREAMING_IO_ERROR;
            }
            ssize_t sz = io_timeout(
                ssize_t, 
                asc_sendfile(str->client_fd, fd, str->total_bytes), 
                timeout, {
                    return STREAMING_TIMEOUT;
                }
            );
            if (sz < 0) {
                mlog_warn("couldn't send to client: %s", strerror(errno));
                return STREAMING_IO_ERROR;
            } 
            str->total_bytes -= sz;
            break;
        }
        default:
            mlog_warn("Transfer method not supported.");
            return STREAMING_IO_ERROR;
    }
    return STREAMING_OK;
}
