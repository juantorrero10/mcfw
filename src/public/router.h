#ifndef ROUTER_H_
#define ROUTER_H_

#include "types.h"
#include "http.h"

#define GET(route_cstr, handler) \
    (router_entry){.route_str = U8_CSTR_SLICE(route_cstr), .method_fptr = handler, .deserialize_typename = {0}, .expect = CONTENT_NOT_SPECIFIED,.r_method = GET }

#define POST(route_cstr, handler) \
    (router_entry){.route_str = U8_CSTR_SLICE(route_cstr), .method_fptr = handler, .deserialize_typename = {0}, .expect = CONTENT_NOT_SPECIFIED, .r_method = POST }

#define POST_JSON(route_cstr, handler, _type) \
    (router_entry){.route_str = U8_CSTR_SLICE(route_cstr), .method_fptr = handler, .deserialize_typename = U8_CSTR_SLICE(#_type), .expect = CONTENT_JSON, .r_method = POST }



typedef struct
{
    request_method r_method;
    http_content_type expect;
    // Can be NULL
    void* method_fptr;
    u8_slice deserialize_typename;
    u8_slice route_str;
} router_entry;


router* router_create(router_entry routes[], size_t amount);

#define router_new(entry_arr) router_create(entry_arr, sizeof(entry_arr) / sizeof(router_entry))

#endif // ROUTER_H_