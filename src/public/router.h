#ifndef ROUTER_H_
#define ROUTER_H_

#include "types.h"
#include "http.h"

typedef http_response* (*route_handler_fn)(http_request*);

#define GET(route_cstr, handler) \
    (router_entry){.route_str = SLICE_CSTR(route_cstr), .method_fptr = handler, .deserialize_typename = {0}, .expect = CONTENT_NOT_SPECIFIED,.r_method = GET }

#define POST(route_cstr, handler) \
    (router_entry){.route_str = SLICE_CSTR(route_cstr), .method_fptr = handler, .deserialize_typename = {0}, .expect = CONTENT_NOT_SPECIFIED, .r_method = POST }

#define POST_JSON(route_cstr, handler, _type) \
    (router_entry){.route_str = SLICE_CSTR(route_cstr), .method_fptr = handler, .deserialize_typename = SLICE_CSTR(#_type), .expect = CONTENT_JSON, .r_method = POST }



typedef struct
{
    request_method r_method;
    http_content_type expect;
    // Can be NULL
    route_handler_fn method_fptr;
    u8_slice deserialize_typename;
    u8_slice route_str;
} router_entry;


router* router_create(router_entry routes[], size_t amount);

#define router_new(entry_arr) router_create(entry_arr, sizeof(entry_arr) / sizeof(router_entry))

#endif // ROUTER_H_