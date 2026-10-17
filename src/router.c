#include <private/mcfw.h>

#define ROUT_RET_ERR(_err) (RoutingResult){.data.err = (_err), .is_ok = false};
#define ROUT_RET_OK(_fptr) (RoutingResult){.data.ok_data = (_fptr), .is_ok = true};


/**
 * @todo hash table
 */
RoutingResult router_get_entry(Router* r, HttpRequest* req) {


    for (size_t i = 0; i < r->entries.size; i++)
    {
        RouterEntry* re = (RouterEntry*)r->entries.array + i;
        if (re->route_str.count != req->path.count) continue;
        if (!re->route_str.s_ptr) continue;
        if (!memcmp(re->route_str.s_ptr, req->path.s_ptr, re->route_str.count)) {
            if (re->r_method != (HttpRequestMethod)req->r_method) return ROUT_RET_ERR(HTTP_METHOD_NOT_ALLOWED);
            return ROUT_RET_OK(re);
        }
    }
    return ROUT_RET_ERR(HTTP_NOT_FOUND);
}



Router* router_create(RouterEntry* entries, size_t amount) {
    RouterEntry* _entries = calloc(sizeof(RouterEntry), amount);
    memcpy(_entries, entries, sizeof(RouterEntry) * amount);
    DHeap routes_pool = DH_NEW(64);
    DHeap typename_pool = DH_NEW(64);
    for (size_t i = 0; i < amount; i++)
    {
        // Push route string
        if (!entries[i].route_str.s_ptr) { 
            mlog_error("Received a null string for route idx: %zu", i);
            DH_FREE(routes_pool);
            DH_FREE(typename_pool);
            return NULL;
        }
        dheap_concat(&routes_pool, &entries[i].route_str);

        // Push typename string if necesary
        if (!(MIME_LUT[entries[i].expect].flags & MIME_FLAG_SERDE)) continue;
        if (!entries[i].deserialize_typename.s_ptr) { 
            mlog_error("Received a null string for type name on route idx: %zu", i);
            DH_FREE(routes_pool);
            DH_FREE(typename_pool);
            return NULL; 
        }
        dheap_concat(&typename_pool, &entries[i].deserialize_typename);

    }
    size_t word_start_route = 0;
    size_t word_end_route = 0;
    size_t word_start_typename = 0;
    size_t word_end_typename = 0;
    for (size_t i = 0; i < amount; i++)
    {

        // Get str offsets for the route
        word_end_route += entries[i].route_str.count;
        _entries[i].route_str = DS_VIEW(routes_pool.h_ptr, word_start_route, word_end_route);
        word_start_route = word_end_route;
    


        // Get for the typename if necesary
        if (!(MIME_LUT[entries[i].expect].flags & MIME_FLAG_SERDE)) continue;
        word_end_typename += entries[i].deserialize_typename.count;
        _entries[i].deserialize_typename = DS_VIEW(typename_pool.h_ptr, word_start_typename, word_end_typename);
        word_start_typename = word_end_typename; 
    }


    Router* ret = malloc(sizeof(Router));
    ret->routes_str_pool = routes_pool;
    ret->types_str_pool = typename_pool;
    ret->entries = (r_array_t){.array = _entries, .chunk_size = sizeof(RouterEntry), .size = amount, .capacity = amount};
    ret->signature = ROUTER_SIGNATURE;
    return ret;
}

void router_destroy(Router* r) {
    if (r->signature != ROUTER_SIGNATURE) return;
    DH_FREE(r->routes_str_pool);
    DH_FREE(r->types_str_pool);
    free(r->entries.array);
}
