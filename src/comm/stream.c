#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <sys/stat.h>
#include <errno.h>

#include <asclib/core.h>
#include <asclib/net/tcp.h>
#include <asclib/io.h>
#include <asclib/sync.h>

#include "../public/types.h"
#include "../private/types.h"
#include "../private/comm.h"
#include "../private/configs.h"
#include "../private/glob.h"
#include "../private/log.h"

#define READ_MAX 8192

#define MIN(a, b) ((a) < (b))? (a) : (b)

void stream_inm(http_stream* out, size_t total_size, u8_slice* start_with, int fd) {
    if (!out) return;
    *out = (http_stream){
        .client_fd = fd,
        .encoding = TRANSFER_INMEDIATE,
        .alignment = ALIGN_HEADER,
        .total_size = total_size,
        .data.inm = VEC_NEW(total_size),
        .evacuate = NULL,
    };
    assert(out->data.inm.data);
    if (start_with) {
        memcpy(out->data.inm.data, start_with->data, start_with->len);
        out->data.inm.count = start_with->len;
    }
}

#define STRET_ERR(__code) (streaming_result){.st = MCFW_ERR, .data.error_code = __code}
#define STRET_OK(buff_ptr) (streaming_result){.st = MCFW_OK, .data.vec = buff_ptr}


/**
 *  @note executed within a async context
 */

static streaming_result __stream_full_inm(http_stream* s) {
    time_t start_time = time(NULL);
    time_t new_time;

    while(s->data.inm.count < s->data.inm.capacity) {
        //mlog_info("iter... tr: %zu", s->data.inm.count);
        //mlog_info("read sz: %zu", MIN(s->data.inm.capacity - s->data.inm.count, READ_MAX));
        streaming_result r = STRET_ERR(HTTP_REQUEST_TIMEOUT);
        ssize_t sz = io_timeout(ssize_t, 
            asc_recv(s->client_fd, s->data.inm.data + s->data.inm.count, 
                MIN(s->data.inm.capacity - s->data.inm.count, READ_MAX)
            ), RECV_BODY_TIMEOUT, 
            {
                return r;
            }
        );
        if (sz < 0) {
            return STRET_ERR(HTTP_INTERNAL_SERVER_ERROR);
        } else if (sz == 0) {
            return STRET_ERR(HTTP_BAD_REQUEST);
        }
        // Check for timeout
        new_time = time(NULL);
        if (new_time - start_time > TOTAL_BODY_TIMEOUT) return STRET_ERR(HTTP_REQUEST_TIMEOUT);

        // Check for 
        s->data.inm.count += sz;
        if (s->total_size > MAX_BODY_SIZE) return STRET_ERR(HTTP_CONTENT_TOO_LARGE);

    }
    s->total_size = s->data.inm.count;
    return STRET_OK(&s->data.inm);
}


inline streaming_result stream_pull_complete(http_stream* s) {
    if (s->encoding == TRANSFER_INMEDIATE) {
        return __stream_full_inm(s);
    }
    return STRET_ERR(HTTP_NOT_IMPLEMENTED);
}


void stream_file(http_stream* out, int client_fd, u8_slice file_name, http_request* req) {
    (void)req;
    if (!out) return;
    *out = (http_stream){
        .client_fd = client_fd,
        .encoding = TRANSFER_FILE,
        .alignment = ALIGN_HEADER,
        .total_size = 0,
        .data.file.ranges_header = http_get_header_val(SLICE_CSTR_LIT("Ranges"), &req->misc_headers),
        .data.file.file_name = file_name,
        .evacuate = NULL,
    };
    int fd = asc_open((char*)file_name.data, O_RDONLY, 0);
    if (fd <= 0) {
        mlog_error("Could not open file '%.*s': %s", SLICE_FMT(file_name), strerror(errno));
        close(fd);
    }
    struct stat st = {0};
    fstat(fd, &st);
    out->data.file.last_modified = st.st_mtime;
    out->data.file.file_fd = fd;
    out->total_size = st.st_size;
}

#define FILE_CHUNK_THRESHOLD 16386 

static streaming_result __stream_file_pc(http_stream* s) {
    if (s->total_size == FILE_CHUNK_THRESHOLD) {
        mlog_warn("Not supported yet");
        // TODO: Use transfer: 
        return STRET_ERR(HTTP_INTERNAL_SERVER_ERROR);
    }
    await(asc_sendfile(s->client_fd, s->data.file.file_fd, s->total_size));
    close(s->data.file.file_fd);
    return STRET_OK(NULL);
}



streaming_result stream_push_complete(http_stream* s) {
    switch (s->encoding) {
    case TRANSFER_FILE:
        return __stream_file_pc(s);
    default:
        return STRET_ERR(HTTP_INTERNAL_SERVER_ERROR);
    }
}