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
    struct StaticDirArray {
        struct StaticDir* data;
        size_t amount;
    } static_dirs;
    DHeap routes_str_pool;
    DHeap types_str_pool;
    DHeap paths_str_pool;
    struct RouterHashMap {
        RouterEntry* data;
        size_t amount;
        size_t capacity;
    } entries;
} Router;

typedef void(*AppEntryFn)(Router* );
 
void router_destroy(Router* r);
HttpStatusCode router_get_entry(Arena* arena, const Router* r, const HttpRequest* req, _Out_ RouterEntry* out_re);
void router_dispatch(const RouterEntry* endpoint, const HttpRequest* req, HttpResponse* res);

#endif // ROUTER_PRIV_H_