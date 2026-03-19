#ifndef PRIVATE_COMM_H_
#define PRIVATE_COMM_H_

#include <time.h>

#include "../public/http.h"
#include "types.h"

typedef enum {
    ALIGN_HEADER,
    ALIGN_CRFL,
    ALIGN_CHUNK,
    ALIGN_COMPLETE,
    ALIGN_TRAILER
} buffer_alignment;

typedef struct {
    transmission_encoding encoding;
    buffer_alignment alignment;
    size_t total_size;
    int client_fd;

    union {
        struct {
            u8_vec buffer;
            size_t chunk_remaining;
        } chunked;

        u8_vec inm;

        // File data
        struct {
            int file_fd;
            u8_slice ranges_header;
            time_t last_modified;
            u8_slice file_name;
        } file;
    } data;
    u8_vec* evacuate;
} http_stream;

void stream_inm(http_stream* out, size_t total_size, u8_slice* start_with, int fd);
/**
 * @note Called withing async context
 */
streaming_result stream_pull_complete(http_stream* s);

streaming_result stream_push_complete(http_stream* s);
void stream_file(http_stream* out, int client_fd, u8_slice file_name, http_request* req);

typedef enum {
    MIME_FLAG_COMPRESS  = 1,
    MIME_FLAG_SERDE     = 1 << 1,
    MIME_FLAG_STATIC    = 1 << 2,
    MIME_FLAG_CACHE     = 1 << 3,
    MIME_FLAG_IS_TEXT   = 1 << 4
} mime_type_flags;

typedef enum {
    MIME_ALGO_NONE,
    MIME_ALGO_GZIP,
    MIME_ALGO_ZSTD,
    MIME_ALGO_BR,
    MIME_ALGO_DEFLATE
} mime_type_compress_algorithm;

typedef struct {
    const char* mime_str;
    const char* full_str;
    const char* ext;
    uint8_t flags;
    mime_type_compress_algorithm comp_algorithm;
} mime_lut_entry;

void res_template_file_nocache(int fd, http_stream* stream, http_content_type type, http_status_code code, int keep_alive);
void res_template_handler(int fd, http_response* res, int keep_alive);
void res_template_host_error(int fd, int code, int keep_alive);

http_response* static_file_handler(http_request* req, u8_slice* file_name, http_content_type t);

#endif // PRIVATE_COMM_H_