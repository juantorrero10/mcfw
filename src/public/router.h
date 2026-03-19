#ifndef ROUTER_H_
#define ROUTER_H_

#include "types.h"
#include "http.h"

typedef enum {
    ROUTER_NONE = -1,
    ROUTER_FUNC,
    ROUTER_FILE
} router_handler_type;

typedef http_response* (*route_handler_fn)(http_request*);

#define GET(route_cstr, _handler) \
    (router_entry){.route_str = SLICE_CSTR(route_cstr), .respond_with = ROUTER_FUNC, .handler.method_fptr = _handler, .deserialize_typename = {0}, .expect = CONTENT_NOT_SPECIFIED,.r_method = GET }

#define GET_FILE(route_cstr, file_path) \
    (router_entry){.route_str = SLICE_CSTR(route_cstr), .respond_with = ROUTER_FILE, .handler.file_name = SLICE_CSTR_LIT(file_path), .deserialize_typename = {0}, .expect = CONTENT_NOT_SPECIFIED,.r_method = GET }

#define POST(route_cstr, _handler) \
    (router_entry){.route_str = SLICE_CSTR(route_cstr), .respond_with = ROUTER_FUNC, .handler.method_fptr = _handler, .deserialize_typename = {0}, .expect = CONTENT_NOT_SPECIFIED, .r_method = POST }

#define POST_JSON(route_cstr, _handler, _type) \
    (router_entry){.route_str = SLICE_CSTR(route_cstr), .respond_with = ROUTER_FUNC, .handler.method_fptr = _handler, .deserialize_typename = SLICE_CSTR(#_type), .expect = CONTENT_JSON, .r_method = POST }




typedef struct
{
    request_method r_method;
    http_content_type expect;
    router_handler_type respond_with;
    union {
        // Can be NULL
        route_handler_fn method_fptr;
        u8_slice file_name;
    } handler;
    u8_slice deserialize_typename;
    u8_slice route_str;
} router_entry;


router* router_create(router_entry routes[], size_t amount);

#define router_new(entry_arr) router_create(entry_arr, sizeof(entry_arr) / sizeof(router_entry))

#endif // ROUTER_H_