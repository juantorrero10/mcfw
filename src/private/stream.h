#ifndef PRIVATE_STREAM_H_
#define PRIVATE_STREAM_H_

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
    size_t total_read;
    int client_fd;

    union {
        struct {
            u8_vec buffer;
            size_t chunk_remaining;
        } chunked;

        u8_vec inm;

        // File data
        struct {
            int store_fd;
            u8_slice file_name;
        } file;
    } data;
    u8_vec* evacuate;
} http_stream;

void stream_inm(http_stream* out, size_t total_size, u8_slice* start_with, int fd);
/**
 * @note Called withing async context
 */
streaming_result stream_build_full_body(http_stream* s);

#endif // PRIVATE_STREAM_H_