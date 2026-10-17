#ifndef ROUTER_H_
#define ROUTER_H_

#include "buffers.h"
#include "http.h"

typedef enum {
    ROUTER_NONE = -1,
    ROUTER_FUNC,
    ROUTER_FILE
} router_handler_type;

typedef void (*RouteHandlerFn)(const HttpRequest*, HttpResponse*);

#define ROUTE_GET(route_cstr, _handler) \
    (RouterEntry){.route_str = DS_CSTR(route_cstr), .respond_with = ROUTER_FUNC, .handler.method_fptr = _handler, .deserialize_typename = {0}, .expect = CONTENT_NOT_SPECIFIED,.r_method = GET }

#define ROUTE_GET_FILE(route_cstr, file_path) \
    (RouterEntry){.route_str = DS_CSTR(route_cstr), .respond_with = ROUTER_FILE, .handler.file_name = DS_CSTR(file_path), .deserialize_typename = {0}, .expect = CONTENT_NOT_SPECIFIED,.r_method = GET }

#define ROUTE_GET_FILE_WITH_TYPE(route_cstr, file_path_cstr, content_type) \
    (RouterEntry){.route_str = DS_CSTR(route_cstr),  .respond_with = ROUTER_FILE, .handler.file_name = DS_CSTR(file_path), .deserialize_typename = {0}, .expect = content_type,.r_method = GET }


#define ROUTE_POST(route_cstr, _handler) \
    (RouterEntry){.route_str = DS_CSTR(route_cstr), .respond_with = ROUTER_FUNC, .handler.method_fptr = _handler, .deserialize_typename = {0}, .expect = CONTENT_NOT_SPECIFIED, .r_method = POST }

#define ROUTE_POST_JSON(route_cstr, _type, _handler) \
    (RouterEntry){.route_str = DS_CSTR(route_cstr), .respond_with = ROUTER_FUNC, .handler.method_fptr = _handler, .deserialize_typename = DS_CSTR(#_type), .expect = CONTENT_JSON, .r_method = POST }




typedef struct
{
    HttpRequestMethod r_method;
    HttpContentType expect;
    router_handler_type respond_with;
    union {
        // Can be NULL
        RouteHandlerFn method_fptr;
        DStatic file_name;
    } handler;
    DStatic deserialize_typename;
    DStatic route_str;
} RouterEntry;

typedef struct __MCFW__ROUTER__ Router;


Router* router_create(RouterEntry routes[], size_t amount);

#define router_new(entry_arr) router_create(entry_arr, sizeof(entry_arr) / sizeof(RouterEntry))

#endif // ROUTER_H_