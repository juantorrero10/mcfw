#ifndef TYPES_H_
#define TYPES_H_

#include <stdint.h>
#include <stddef.h>
#include <crefl/types.h>

REFLECT(enum) {
    HTTP_OK = 200,
    HTTP_BAD_REQUEST = 400,
    HTTP_NOT_FOUND = 404,
    HTTP_METHOD_NOT_ALLOWED = 405,
    HTTP_INTERNAL_SERVER_ERROR = 500
} http_status_code;

typedef enum {
    APP_TEXT,
    APP_HTML,
    APP_JSON,
} http_content_type;


typedef struct {
    http_status_code code;
    http_content_type type;
    void* buff;
} http_response;

typedef struct {
    //headers
    http_content_type type;
    void* buff;
} http_request;

typedef http_response (*route_method)(http_request);

typedef void* (*async_fn)(void*);

REFLECT(enum) {
    GET,
    POST,
    PUT,
    PATCH,
    DELETE,
    OPTIONS,
    HEAD
} request_kind;

REFLECT(struct)
{
    char* route;
    request_kind r_kind;
    void* method_fptr;
} router_entry;

#define ROUTER_SIGNATURE 0x524F5452 // "ROTR"

REFLECT(struct) {
    uint32_t signature;
    DYN_ARRAY(router_entry) entries;
} router;

typedef enum {
    MCFW_OK,
    MCFW_ERR,
} mcfw_status;

typedef struct {
    mcfw_status st;
    union {
        async_fn method_fptr;
        http_status_code err;
    } data;
} routing_result;



#endif // TYPES_H_