#ifndef TYPES_H_
#define TYPES_H_

#include <stdint.h>
#include <stddef.h>

typedef enum {
    HTTP_OK = 200,
    HTTP_NOT_FOUND = 404,
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

typedef http_response (*route_method)(http_content_type);

typedef void* (*async_fn)(void*);

typedef struct
{
    char* route;
    route_method fptr;
} router_entry;

#define ROUTER_SIGNATURE 0x524F5452 // "ROTR"

typedef struct {
    uint32_t signature;
    router_entry* entries;
    size_t count;
    size_t capacity;
} router;



#endif // TYPES_H_