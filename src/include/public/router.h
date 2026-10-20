#ifndef ROUTER_H_
#define ROUTER_H_

#include "buffers.h"
#include "http.h"
#include <assert.h>

typedef enum {
    ROUTER_NONE = -1,
    ROUTER_FUNC,
    ROUTER_FILE,
    ROUTER_DIR,
} RouteHandlerKind;

typedef void (*RouteHandlerFn)(const HttpRequest*, HttpResponse*);

/**
 * @todo path parameters
 */
typedef void (*RouteHandlerUIntFn)(const HttpRequest*, HttpResponse*, uint64_t);
typedef void (*RouteHandlerIntFn)(const HttpRequest*, HttpResponse*, int64_t);
typedef void (*RouteHandlerStrFn)(const HttpRequest*, HttpResponse*, DStatic);

typedef struct {
    bool deserialize;
    HttpRequestMethod r_method;
    HttpContentType expect;
    struct {
        RouteHandlerKind kind;
        union {
            RouteHandlerFn method_fptr;
            DStatic path;
        };
    } handler;
    DStatic deserialize_typename;
    DStatic route_str;
} RouterEntry;

typedef struct __MCFW__ROUTER__ Router;

#ifdef ROUTE_STRINGS_DSTATIC
    #define STR_CONVERSION (DStatic)
#else
    #define STR_CONVERSION DS_LIT
#endif

#define HANDLER_FN(func) { .kind = ROUTER_FUNC, .method_fptr = (func) }

#define HANDLER_FILE(file_path_cstr) { .kind = ROUTER_FILE, .path = STR_CONVERSION(file_path_cstr) }

#define HANDLER_DIR(dir_path_cstr) { .kind = ROUTER_DIR, .path = STR_CONVERSION(dir_path_cstr) }


#define ROUTE(_method, _route_cstr, _handler)       \
    (RouterEntry){                                  \
        .deserialize = false,                       \
        .r_method = (_method),                      \
        .route_str = STR_CONVERSION(_route_cstr),   \
        .expect = CONTENT_NOT_SPECIFIED,            \
        .deserialize_typename = DS_EMPTY(),         \
        .handler = _handler                         \
    }

#define ROUTE_DESERIALIZE(_method, _route_cstr, _handler, _content_type, _reflected_type) \
    ({                                                      \
        static_assert((int)_method >= PAYLOAD_METHODS_IDX); \
        RouterEntry __val = (RouterEntry){                  \
        .deserialize = true,                                \
        .r_method = (_method),                              \
        .route_str = STR_CONVERSION(_route_cstr),           \
        .expect = (_content_type),                          \
        .deserialize_typename = DS_LIT(#_reflected_type),   \
        .handler = _handler                                 \
        };                                                  \
        __val;                                              \
    })
    




void __router_set_routes(Router* r, RouterEntry routes[], size_t amount);

#define router_set_routes(_rout_ptr, ...) do {\
    RouterEntry __rarr[] = { __VA_ARGS__ }; \
    __router_set_routes(_rout_ptr, __rarr, sizeof(__rarr) / sizeof(RouterEntry)); \
} while (0)


#endif // ROUTER_H_