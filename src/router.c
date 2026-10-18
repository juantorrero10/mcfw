#include <private/mcfw.h>

#define ROUT_RET_ERR(_err) (RoutingResult){.data.err = (_err), .is_ok = false};
#define ROUT_RET_OK(_fptr) (RoutingResult){.data.ok_data = (_fptr), .is_ok = true};

static inline uint32_t fnv1a_hash(const DStatic* str) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < str->count; i++) {
        uint8_t c = ((uint8_t*)str->s_ptr)[i];
        if (c >= 'A' && c <= 'Z') c |= 0x20;
        hash ^= c;
        hash *= 16777619u;
    }
    return hash;
}

RoutingResult router_get_entry(Router* r, HttpRequest* req) {
    size_t n_attempt = 0;
    size_t hashed_idx = fnv1a_hash(&req->path) % r->entries.capacity;
    while(n_attempt < r->entries.capacity)
    {
        RouterEntry* re = (RouterEntry*)r->entries.array + hashed_idx;
        hashed_idx++;
        hashed_idx %= r->entries.capacity;
        n_attempt++;
        if (!DS_IS_VALID(re->route_str)) break; 
        if (re->route_str.count != req->path.count) continue;
        if (!strncasecmp(re->route_str.s_ptr, req->path.s_ptr, re->route_str.count)) {
            if (re->r_method != (HttpRequestMethod)req->r_method) return ROUT_RET_ERR(HTTP_METHOD_NOT_ALLOWED);
            return ROUT_RET_OK(re);
        }
    }
    return ROUT_RET_ERR(HTTP_NOT_FOUND);
}



Router* router_create(RouterEntry* entries, size_t amount) {
    size_t entries_cap = 2 * amount;
    RouterEntry* _entries = calloc(sizeof(RouterEntry), entries_cap);
    memset(_entries, 0, entries_cap * sizeof(RouterEntry));
    DHeap routes_str_pool = DH_NEW(64);
    DHeap typename_str_pool = DH_NEW(64);
    for (size_t i = 0; i < amount; i++)
    {
        // Push route string
        if (!entries[i].route_str.s_ptr) { 
            mlog_error("Received a null string for route idx: %zu", i);
            DH_FREE(routes_str_pool);
            DH_FREE(typename_str_pool);
            return NULL;
        }
        dheap_concat(&routes_str_pool, &entries[i].route_str);

        // Push typename string if necesary
        if (!(MIME_LUT[entries[i].expect].flags & MIME_FLAG_SERDE)) continue;
        if (!entries[i].deserialize_typename.s_ptr) { 
            mlog_error("Received a null string for type name on route idx: %zu", i);
            DH_FREE(routes_str_pool);
            DH_FREE(typename_str_pool);
            return NULL; 
        }
        dheap_concat(&typename_str_pool, &entries[i].deserialize_typename);

    }
    size_t word_start_route = 0;
    size_t word_end_route = 0;
    size_t word_start_typename = 0;
    size_t word_end_typename = 0;
    for (size_t i = 0; i < amount; i++)
    {
        // Find new hashtable idx
        const RouterEntry* old_entry = &entries[i];
        size_t hashed_idx = (size_t)fnv1a_hash(&old_entry->route_str) % entries_cap;
        while(DS_IS_VALID(_entries[hashed_idx].route_str)) hashed_idx = (hashed_idx + 1) % entries_cap;
        RouterEntry* new_entry = &_entries[hashed_idx];
        memcpy(new_entry, old_entry, sizeof(RouterEntry));
        
        // Get str offsets for the route
        word_end_route += old_entry->route_str.count;
        new_entry->route_str = DS_VIEW(routes_str_pool.h_ptr, word_start_route, word_end_route);
        word_start_route = word_end_route;
    


        // Get for the typename if necesary
        if (!(MIME_LUT[entries[i].expect].flags & MIME_FLAG_SERDE)) continue;
        word_end_typename += entries[i].deserialize_typename.count;
        new_entry->deserialize_typename = DS_VIEW(typename_str_pool.h_ptr, word_start_typename, word_end_typename);
        word_start_typename = word_end_typename; 
    }


    Router* ret = malloc(sizeof(Router));
    ret->routes_str_pool = routes_str_pool;
    ret->types_str_pool = typename_str_pool;
    ret->entries = (r_array_t){.array = _entries, .chunk_size = sizeof(RouterEntry), .size = amount, .capacity = entries_cap};
    ret->signature = ROUTER_SIGNATURE;
    return ret;
}

void router_destroy(Router* r) {
    if (r->signature != ROUTER_SIGNATURE) return;
    DH_FREE(r->routes_str_pool);
    DH_FREE(r->types_str_pool);
    free(r->entries.array);
}
