#include <private/mcfw.h>

#define ROUT_RET_ERR(_err) (RoutingResult){.data.err = (_err), .is_ok = false}
#define ROUT_RET_OK(_fptr) (RoutingResult){.data.ok_data = (_fptr), .is_ok = true}

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

// Match longest  prefix
static HttpStatusCode router_get_file_static_dir(
    Arena* arena, 
    const Router* r, 
    const HttpRequest* req, 
    _Out_ DStatic* file_path
) {
    if (r->static_dirs.amount == 0) return HTTP_NOT_FOUND;

    const struct StaticDir* best_match = NULL;
    size_t max_prefix_len = 0;
    
    for (size_t i = 0; i < r->static_dirs.amount; i++) {
        const struct StaticDir* s = &r->static_dirs.data[i];
        
        if (req->path.count < s->prefix.count) continue; 

        if (memcmp(req->path.s_ptr, s->prefix.s_ptr, s->prefix.count) == 0) {
            if (s->prefix.count > max_prefix_len) {
                // Track longest one
                max_prefix_len = s->prefix.count;
                best_match = s;
            }
        }
    }

    if (!best_match) return HTTP_NOT_FOUND;
    if (best_match->method != req->r_method) return HTTP_METHOD_NOT_ALLOWED;

    // Concat the dir path with the request path - the prefix
    DStatic no_prefix = DS_VIEW((uint8_t*)req->path.s_ptr, best_match->prefix.count, req->path.count);
    
    // Allocate buffer
    size_t sz_needed = no_prefix.count + best_match->dir_path.count;
    // Out of memory
    if (arena->used + sz_needed + 7 > arena->alloc_size) {
        return HTTP_INTERNAL_SERVER_ERROR;
    }
    *file_path = mcfw_arena_alloc(arena, sz_needed, true);
    dstatic_concat(file_path, &best_match->dir_path);
    dstatic_concat(file_path, &no_prefix);

    return HTTP_OK;
}


#define DS_COMP_CASEIGN(_d1, _d2) ((_d1).count != (_d2).count || strncasecmp((_d1).s_ptr, (_d2).s_ptr, (_d1).count))

HttpStatusCode router_get_entry(Arena* arena, const Router* r, const HttpRequest* req, _Out_ RouterEntry* out_re) {
    size_t n_attempt = 0;
    size_t hashed_idx = fnv1a_hash(&req->path) % r->entries.capacity;
    int found_same_path = 0;
    while(n_attempt < r->entries.capacity)
    {
        RouterEntry* re = (RouterEntry*)r->entries.array + hashed_idx;
        hashed_idx++;
        hashed_idx %= r->entries.capacity;
        n_attempt++;
        if (!DS_IS_VALID(re->route_str)) break;
        if (DS_COMP_CASEIGN(re->route_str, req->path)) continue;
        if (re->r_method != (HttpRequestMethod)req->r_method) {
            found_same_path = 1;
            continue;
        }
        memcpy(out_re, re, sizeof(RouterEntry));
        return HTTP_OK;
    }
    if (!found_same_path) {
        HttpStatusCode s = router_get_file_static_dir(arena, r, req, &out_re->handler.path);
        if (s == HTTP_OK) {
            out_re->handler.kind = ROUTER_FILE;
            out_re->expect = CONTENT_NOT_SPECIFIED;
        }
        return s;
    }
    return HTTP_METHOD_NOT_ALLOWED;
}




Router* router_create(RouterEntry* entries, size_t amount) {
    size_t entries_cap = 2 * amount;
    DHeap routes_str_pool = DH_NEW(64);
    DHeap typename_str_pool = DH_NEW(64);
    DHeap paths_str_pool = DH_NEW(64);
    size_t n_staticdirs = 0;
    for (size_t i = 0; i < amount; i++)
    {
        // Push route string
        if (!entries[i].route_str.s_ptr) { 
            mlog_error("Expected route path but received a null string on route idx: %zu", i);
            DH_FREE(routes_str_pool);
            DH_FREE(typename_str_pool);
            DH_FREE(paths_str_pool);
            return NULL;
        }
        dheap_concat(&routes_str_pool, &entries[i].route_str);

        // Push file/dir path 
        RouteHandlerKind k = entries[i].handler.kind;
        if (k == ROUTER_FILE || k == ROUTER_DIR) {
            if (!DS_IS_VALID(entries[i].handler.path)) {
                mlog_error("Expected file/dir path but received a null string on route idx: %zu", i);
                DH_FREE(routes_str_pool);
                DH_FREE(typename_str_pool);
                DH_FREE(paths_str_pool);
                return NULL;
            }
            dheap_concat(&paths_str_pool, &entries[i].handler.path);

            if (k == ROUTER_DIR) {
                // Count static dirs
                n_staticdirs++;
                // Append / if not there
                if (((char*)(entries[i].handler.path.s_ptr))[entries[i].handler.path.count - 1] != '/') {
                    DStatic b = DS_LIT("/");
                    dheap_concat(&paths_str_pool, &b);
                    entries[i].handler.path.count++;
                }
            }
        }

        // Push typename string if necesary
        if (!(MIME_LUT[entries[i].expect].flags & MIME_FLAG_SERDE)) continue;
        if (!entries[i].deserialize_typename.s_ptr) { 
            mlog_error("Expected type name but received a null string on route idx: %zu", i);
            DH_FREE(routes_str_pool);
            DH_FREE(typename_str_pool);
            DH_FREE(paths_str_pool);
            return NULL; 
        }
        dheap_concat(&typename_str_pool, &entries[i].deserialize_typename);


    }
    RouterEntry* _entries = calloc(entries_cap, sizeof(RouterEntry));
    struct StaticDir* sdirs = (n_staticdirs == 0)? NULL : calloc(n_staticdirs, sizeof(struct StaticDir));
    size_t sdirs_count = 0;

    size_t word_start_route = 0;
    size_t word_end_route = 0;
    size_t word_start_typename = 0;
    size_t word_end_typename = 0;
    size_t word_start_path = 0;
    size_t word_end_path = 0;
    for (size_t i = 0; i < amount; i++)
    {
        // Find new hashtable idx
        const RouterEntry* old_entry = &entries[i];
        size_t hashed_idx = (size_t)fnv1a_hash(&old_entry->route_str) % entries_cap;
        while(DS_IS_VALID(_entries[hashed_idx].route_str)) hashed_idx = (hashed_idx + 1) % entries_cap;
        RouterEntry* new_entry = &_entries[hashed_idx];
        memcpy(new_entry, old_entry, sizeof(RouterEntry));
        
        RouteHandlerKind k = entries[i].handler.kind;

        // Get str offsets for the route
        word_end_route += old_entry->route_str.count;
        DStatic route_str = DS_VIEW(routes_str_pool.h_ptr, word_start_route, word_end_route);
        word_start_route = word_end_route;

        if (k != ROUTER_DIR) {
            new_entry->route_str = route_str;
        }

        // Get str offsets for the path
        if (k == ROUTER_FILE || k == ROUTER_DIR) {
            word_end_path += old_entry->handler.path.count;
            DStatic path = DS_VIEW(paths_str_pool.h_ptr, word_start_path, word_end_path);
            if (k == ROUTER_FILE) {
                new_entry->handler.path = path;
            } else {
                sdirs[sdirs_count].method = old_entry->r_method;
                sdirs[sdirs_count].prefix = route_str;
                sdirs[sdirs_count++].dir_path = path;
            }
            word_start_path = word_end_path;
        }


        // Get for the typename if necesary
        if (!(MIME_LUT[entries[i].expect].flags & MIME_FLAG_SERDE)) continue;
        word_end_typename += entries[i].deserialize_typename.count;
        new_entry->deserialize_typename = DS_VIEW(typename_str_pool.h_ptr, word_start_typename, word_end_typename);
        word_start_typename = word_end_typename; 
    }


    Router* ret = malloc(sizeof(Router));
    ret->routes_str_pool = routes_str_pool;
    ret->types_str_pool = typename_str_pool;
    ret->paths_str_pool = paths_str_pool;
    ret->entries = (r_array_t){.array = _entries, .chunk_size = sizeof(RouterEntry), .size = amount, .capacity = entries_cap};
    ret->static_dirs.data = sdirs;
    ret->static_dirs.amount = n_staticdirs;
    ret->signature = ROUTER_SIGNATURE;
    return ret;
}

void router_destroy(Router* r) {
    if (r->signature != ROUTER_SIGNATURE) return;
    DH_FREE(r->routes_str_pool);
    DH_FREE(r->types_str_pool);
    DH_FREE(r->paths_str_pool);
    free(r->entries.array);
    free(r->static_dirs.data);
}
