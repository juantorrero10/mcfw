#ifndef TYPES_H_
#define TYPES_H_

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <crefl/types.h>

typedef struct {
    uint8_t* data;
    size_t count;
    size_t capacity;
} u8_vec;

#define U8_VEC_NULL (u8_vec){0}


#define U8_VEC(_buffer, _size) ({\
    (u8_vec){.data = (uint8_t*)_buffer, .count = _size, .capacity = _size };\
})

#define U8_VEC_NEW(_buffer, _cap) ({\
    (u8_vec){.data = (uint8_t*)_buffer, .count = 0, .capacity = _cap };\
})

#define U8_VEC_STR(_buffer) ({\
    size_t __s = strlen(_buffer);\
    U8_VEC(_buffer, __s);\
})

#define U8_VEC_SUBSTR(_buffer, start, end) ({\
    U8_VEC(_buffer + start, end - start);\
})

#define U8_VEC_FORMAT(vec) (int)((vec).count), (vec).data

REFLECT(enum) {
    HTTP_OK = 200,
    HTTP_BAD_REQUEST = 400,
    HTTP_GONE = 403,
    HTTP_NOT_FOUND = 404,
    HTTP_METHOD_NOT_ALLOWED = 405,
    HTTP_INTERNAL_SERVER_ERROR = 500,
    HTTP_SERVICE_UNAVAILABLE = 503
} http_status_code;

typedef enum {
    APP_TEXT,
    APP_HTML,
    APP_JSON,
} http_content_type;

typedef struct __MCFW_HTTP_RESPONSE__ http_response;

typedef struct {
    //headers
    http_content_type type;
    void* buff;
} http_request;

REFLECT(enum) {
    GET,
    POST,
    PUT,
    PATCH,
    DELETE,
    OPTIONS,
    HEAD
} request_kind;

typedef struct
{
    u8_vec route_str;
    request_kind r_kind;
    void* method_fptr;
} router_entry;

typedef struct __MCFW__ROUTER__ router;



#endif // TYPES_H_