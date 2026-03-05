#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

#include <asclib/core.h>
#include <asclib/net/tcp.h>
#include <asclib/sync.h>

#include "../public/types.h"
#include "../private/types.h"
#include "../private/stream.h"
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
        .total_read = 0,
        .data.inm = VEC_NEW(total_size),
        .evacuate = NULL,
    };
    assert(out->data.inm.data);
    if (start_with) {
        mlog_info("data: %p, other: %p, len: %zu", out->data.inm.data, start_with->data, start_with->len);
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
        mlog_info("iter... tr: %zu", s->data.inm.count);
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
        if (s->total_read > MAX_BODY_SIZE) return STRET_ERR(HTTP_CONTENT_TOO_LARGE);

    }
    s->total_read = s->data.inm.count;
    mlog_info("clear!");
    return STRET_OK(&s->data.inm);
} 


inline streaming_result stream_build_full_body(http_stream* s) {
    mlog_info("a");
    if (s->encoding == TRANSFER_INMEDIATE) {
        return __stream_full_inm(s);
    }
    return STRET_ERR(HTTP_NOT_IMPLEMENTED);
}