#include <string.h>
#include <errno.h>
#include <time.h>

#include <crefl/core.h>

#include <asclib/core.h>
#include <asclib/sync.h>
#include <asclib/io.h>
#include <asclib/net/tcp.h>

#include "../public/http.h"
#include "../private/configs.h"
#include "../private/glob.h"
#include "../private/comm.h"
#include "../private/http.h"

#include "../private/log.h"


static inline char* get_gmt_date(char* write_at, size_t len) {
    time_t now = time(NULL);
    struct tm *gmt = gmtime(&now);
    // %a = Abbreviated weekday (Sun)
    // %d = Day of month (01)
    // %b = Abbreviated month (Mar)
    // %Y = Year (2026)
    // %H:%M:%S = Time
    strftime(write_at, len, "%a, %d %b %Y %H:%M:%S GMT", gmt);
    return write_at;
}



#define SEND_TIMEOUT 2000
#define STRIP_HTTP_PREFIX(_str_expr) (_str_expr + 5)

/**
 * @brief Send a bodyless error back to the client.
 * This is when 500's, 404's, 405's and simple 400's are sent.
 */
void res_template_host_error(int fd, int code, int keep_alive) {
    uint8_t __r[MAX_HEADER_SIZE];
    u8_vec buffer = VEC_FROM(__r, 0, sizeof(__r));

    const char* str_status = refl_enum_get_label((REnum*)CACHED_ENUM_STR_STATUS, code);
    if (!str_status) str_status = "INTERNAL_SERVER_ERROR";

    char* fmt_template = 
        "HTTP/1.1 %03d %s\r\n"
        "Server: "SERVER_HEADER_VAL"\r\n"
        "Connection: %s\r\n"
        "Content-Length: 0\r\n"
        "\r\n"
    ;
    buffer.count = snprintf((char*)buffer.data, buffer.capacity, fmt_template, code, 
            STRIP_HTTP_PREFIX(str_status), (keep_alive)? "keep-alive" : "close");

    ssize_t sz = io_timeout(ssize_t, asc_send(fd, buffer.data, buffer.count), SEND_TIMEOUT, {
        mlog_warn("<%d> timed out", fd);
        return;
    });
    if (sz < 0) {
        mlog_warn("couldn't send to client: %s", strerror(errno));
    }
}

void res_template_handler(int fd, http_response* res, int keep_alive) {

    uint8_t __r[MAX_HEADER_SIZE];
    u8_vec buffer = VEC_FROM(__r, 0, sizeof(__r));
    // Get content type header
    char* ctype_str = (res->type > 0 && res->type < CONTENT_TYPE_N_MEMB)? (char*)MIME_LUT[res->type].full_str : NULL;
    if (ctype_str == NULL) ctype_str = "application/octet-stream";

    // Get string version of the error code.
    const char* str_status = refl_enum_get_label((REnum*)CACHED_ENUM_STR_STATUS, res->code);
    if (!str_status) str_status = "OK"; // Fallback case.

    char date_buff[32];

    // TODO: vec push

    char* fmt_template = 
        "HTTP/1.1 %03d %s\r\n"
        "Server: "SERVER_HEADER_VAL"\r\n"
        "Date: %s\r\n"
        "Connection: %s\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "\r\n"
        ;
    buffer.count = snprintf((char*)buffer.data, buffer.capacity, fmt_template, 
        res->code, STRIP_HTTP_PREFIX(str_status), get_gmt_date(date_buff, 32), (keep_alive)? "keep-alive" : "close",
        ctype_str, res->stream.total_size);

    // If somehow headers get too heavy and exceed HEADER_MAX_SIZE - 2 (trailing \r\n)
    if (buffer.count >= buffer.capacity - 2) {
        res_template_host_error(fd, HTTP_INTERNAL_SERVER_ERROR, 0);
        return;
    }


    struct iovec vecs[2] = {
        // Headers
        {.iov_base = buffer.data, .iov_len = buffer.count},
        // Body
        {.iov_base = res->stream.data.inm.data, .iov_len = res->stream.data.inm.count}
    };


    struct msghdr full_msg = {
        .msg_iov = vecs, .msg_iovlen = 2
    };

    ssize_t sz = io_timeout(ssize_t, asc_sendmsg(fd, &full_msg), SEND_TIMEOUT, {
        return;
    });
    if (sz < 0) {
        mlog_warn("couldn't send to client: %s", strerror(errno));
    }
}

#define FILE_CHUNK_THRESHOLD 16386
extern mime_lut_entry MIME_LUT[CONTENT_TYPE_N_MEMB];

void res_template_file_nocache(int fd, http_stream* stream, http_content_type type, http_status_code code, int keep_alive) {
    uint8_t __r[MAX_HEADER_SIZE];
    u8_vec buffer = VEC_FROM(__r, 0, sizeof(__r));
    // Get content type header
    const char* ctype_str = (type > 0 && type < CONTENT_TYPE_N_MEMB)? MIME_LUT[type].full_str : NULL;
    if (ctype_str == NULL) ctype_str = "application/octet-stream";

    // Get string version of the error code.
    const char* str_status = refl_enum_get_label((REnum*)CACHED_ENUM_STR_STATUS, type);
    if (!str_status) str_status = "OK"; // Fallback case.

    char date_buff[32];

    char* fmt_template = NULL;

    if (stream->total_size > FILE_CHUNK_THRESHOLD) {
        fmt_template = 
            "HTTP/1.1 %03d %s\r\n"
            "Server: "SERVER_HEADER_VAL"\r\n"
            "Date: %s\r\n"
            "Connection: %s\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Type: %s\r\n"
            "Transfer-Encoding: Chunked\r\n"
            "\r\n"
            ;

        buffer.count = snprintf((char*)buffer.data, buffer.capacity, fmt_template, 
        code, STRIP_HTTP_PREFIX(str_status), get_gmt_date(date_buff, 32), (keep_alive)? "keep-alive" : "close",
        ctype_str);
    } else {
        fmt_template = 
            "HTTP/1.1 %03d %s\r\n"
            "Server: "SERVER_HEADER_VAL"\r\n"
            "Date: %s\r\n"
            "Connection: %s\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %zu\r\n"
            "\r\n"
            ;
        buffer.count = snprintf((char*)buffer.data, buffer.capacity, fmt_template, 
        code, STRIP_HTTP_PREFIX(str_status), get_gmt_date(date_buff, 32), (keep_alive)? "keep-alive" : "close",
        ctype_str, stream->total_size);
    }

    if (buffer.count >= buffer.capacity - 2) {
        res_template_host_error(fd, HTTP_INTERNAL_SERVER_ERROR, 0);
        return;
    }
    
    ssize_t sz = io_timeout(ssize_t, asc_send(fd, buffer.data, buffer.count), SEND_TIMEOUT, {
        return;
    });
    if (sz < 0) {
        mlog_warn("couldn't send to client: %s", strerror(errno));
    }
    
}