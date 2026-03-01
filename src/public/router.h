#ifndef ROUTER_H_
#define ROUTER_H_

#include "types.h"

#define GET(route_cstr, handler) \
    (router_entry){.route_str = U8_STR_SLICE(route_cstr), .method_fptr = handler, .deserialize_typename = {0}, .expect = CONTENT_TEXT,.r_kind = GET }

#define POST(route_cstr, handler) \
    (router_entry){.route_str = U8_STR_SLICE(route_cstr), .method_fptr = handler, .deserialize_typename = {0}, .expect = CONTENT_TEXT, .r_kind = POST }

#define POST_JSON(route_cstr, handler, _type) \
    (router_entry){.route_str = U8_STR_SLICE(route_cstr), .method_fptr = handler, .deserialize_typename = U8_STR_SLICE(#_type), .expect = CONTENT_JSON, .r_kind = POST }




router* router_create(router_entry routes[], size_t amount);

#define router_new(entry_arr) router_create(entry_arr, sizeof(entry_arr) / sizeof(router_entry))

#endif // ROUTER_H_