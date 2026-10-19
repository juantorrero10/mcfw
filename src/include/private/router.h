#ifndef ROUTER_PRIV_H_
#define ROUTER_PRIV_H_

#include <public/router.h>
#include <public/http.h>


#define ROUTER_SIGNATURE 0x524F5452 // "ROTR"

struct StaticDir {
    HttpRequestMethod method;
    DStatic prefix;
    DStatic dir_path;
};

typedef struct __MCFW__ROUTER__ {
    DHeap routes_str_pool;
    DHeap types_str_pool;
    DHeap paths_str_pool;
    uint32_t signature;
    DYN_ARRAY(RouterEntry) entries;
    struct {
        struct StaticDir* data;
        size_t amount;
    } static_dirs;
} Router;

typedef Router* (*AppEntryFn)(void);
 
void router_destroy(Router* r);
HttpStatusCode router_get_entry(Arena* arena, const Router* r, const HttpRequest* req, _Out_ RouterEntry* out_re);

#endif // ROUTER_PRIV_H_