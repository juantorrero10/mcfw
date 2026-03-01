#ifndef TYPES_H_
#define TYPES_H_

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <crefl/types.h>
#include <stdbool.h>

typedef struct {
    uint8_t* data;
    size_t count;
    size_t capacity;
} u8_vec;

typedef struct {
    uint8_t* data;
    size_t len;
} u8_slice;

#define U8_VEC_NULL (u8_vec){0}


#define U8_VEC(_buffer, _size) ({\
    void* __p = malloc(_size);\
    memcpy(__p, (_buffer), (_size));\
    (u8_vec){.data = (uint8_t*)__p, .count = _size, .capacity = _size };\
})

#define U8_VEC_NEW(_cap) ({\
    (u8_vec){.data = (uint8_t*)malloc(_cap), .count = 0, .capacity = _cap };\
})

#define U8_VEC_STR(_null_term_str) ({\
    size_t __s = strlen((_null_term_str));\
    U8_VEC((_null_term_str), __s);\
})

#define U8_VEC_FREE(_vec) free((_vec).data)

#define U8_SLICE(_buffer, _len) ({\
    (u8_slice){.data = (uint8_t*)(_buffer), .len = (_len)};\
})

#define U8_STR_SLICE(_null_term_str) ({\
    (u8_slice){.data = (uint8_t*)_null_term_str, .len = strlen(_null_term_str)};\
})

#define U8_VIEW(_buffer, start, end) ({\
    U8_SLICE(_buffer + start, end - start);\
})

#define U8_VEC_FMT(vec) (int)((vec).count), (vec).data
#define U8_SLICE_FMT(slice) (int)(slice).len, (slice).data

REFLECT(enum) {
    HTTP_OK = 200,
    HTTP_BAD_REQUEST = 400,
    HTTP_FORBIDDEN = 403,
    HTTP_NOT_FOUND = 404,
    HTTP_METHOD_NOT_ALLOWED = 405,
    HTTP_GONE = 410,
    HTTP_UNPROCESABLE_CONTENT = 422,
    HTTP_INTERNAL_SERVER_ERROR = 500,
    HTTP_SERVICE_UNAVAILABLE = 503
} http_status_code;

REFLECT(enum) {
    CONTENT_TEXT,
    CONTENT_HTML,
    CONTENT_JSON,
} http_content_type;

typedef struct __MCFW_HTTP_RESPONSE__ http_response;

#define PAYLOAD_METHODS_IDX 3

REFLECT(enum) {
    GET,
    DELETE,
    HEAD,
    POST = 3,
    PUT,
    PATCH,
    OPTIONS,
} request_kind;

typedef struct {
    //headers
    int http_version;
    request_kind r_kind;
    http_content_type type;
    size_t content_length;
    
    u8_slice path;
    // string view of the body inside the full request -> If no body pointer is NULL!!
    u8_slice body;
    /**
     * @brief heap allocated decoded data extracted from the request: ex. deserialized struct from a JSON POST
     * It will be NULL if there's nothing to decode. 
     * @attention This buffer is freed automatically, don't call free() on it
    */ 
    u8_vec decoded;
} http_request;

typedef struct
{
    request_kind r_kind;
    http_content_type expect;
    // Can be NULL
    void* method_fptr;
    u8_slice deserialize_typename;
    u8_slice route_str;
} router_entry;

typedef struct __MCFW__ROUTER__ router;



#endif // TYPES_H_