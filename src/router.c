#define _GNU_SOURCE

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>

#include <crefl/core.h>
#include <crefl/scope.h>
#include <crefl/serial/formats.h>

#include "private/types.h"
#include "private/log.h"
#include "private/router.h"
#include "private/version.h"
#include "private/glob.h"

#define ROUT_RET_ERR(_err) (routing_result){.data.err = (_err), .st = MCFW_ERR};
#define ROUT_RET_OK(_fptr) (routing_result){.data.ok_data = (_fptr), .st = MCFW_OK};


/**
 * @todo hash table
 */
routing_result router_get_entry(router* r, http_request* req) {


    for (size_t i = 0; i < r->entries.size; i++)
    {
        router_entry* re = (router_entry*)r->entries.array + i;
        if (re->route_str.len != req->path.len) continue;
        if (!re->route_str.data) continue;
        if (!memcmp(re->route_str.data, req->path.data, re->route_str.len)) {
            if (re->r_method != (request_method)req->r_method) return ROUT_RET_ERR(HTTP_METHOD_NOT_ALLOWED);
            return ROUT_RET_OK(re);
        }
    }
    return ROUT_RET_ERR(HTTP_NOT_FOUND);
}

static void u8_vec_push(u8_vec* v, u8_slice* s) {
    if (v->count + s->len >= v->capacity) {
        v->capacity *= 2;
        v->data = realloc(v->data, v->capacity);
    }
    memcpy(v->data + v->count, s->data, s->len);
    v->count += s->len;
}

router* router_create(router_entry* entries, size_t amount) {
    router_entry* _entries = calloc(sizeof(router_entry), amount);
    memcpy(_entries, entries, sizeof(router_entry) * amount);
    u8_vec routes_pool = VEC_NEW(64);
    u8_vec typename_pool = VEC_NEW(64);
    for (size_t i = 0; i < amount; i++)
    {
        // Push route string
        if (!entries[i].route_str.data) { 
            mlog_error("Received a null string for route idx: %zu", i);
            VEC_FREE(routes_pool);
            VEC_FREE(typename_pool);
            return NULL;
        }
        u8_vec_push(&routes_pool, &entries[i].route_str);

        // Push typename string if necesary
        if (!(MIME_LUT[entries[i].expect].flags & MIME_FLAG_SERDE)) continue;
        if (!entries[i].deserialize_typename.data) { 
            mlog_error("Received a null string for type name on route idx: %zu", i);
            VEC_FREE(routes_pool);
            VEC_FREE(typename_pool);
            return NULL; 
        }
        u8_vec_push(&typename_pool, &entries[i].deserialize_typename);

    }
    size_t word_start_route = 0;
    size_t word_end_route = 0;
    size_t word_start_typename = 0;
    size_t word_end_typename = 0;
    for (size_t i = 0; i < amount; i++)
    {

        // Get str offsets for the route
        word_end_route += entries[i].route_str.len;
        _entries[i].route_str = SLICE_VIEW(routes_pool.data, word_start_route, word_end_route);
        word_start_route = word_end_route;
    


        // Get for the typename if necesary
        if (!(MIME_LUT[entries[i].expect].flags & MIME_FLAG_SERDE)) continue;
        word_end_typename += entries[i].deserialize_typename.len;
        _entries[i].deserialize_typename = SLICE_VIEW(typename_pool.data, word_start_typename, word_end_typename);
        word_start_typename = word_end_typename; 
    }


    router* ret = malloc(sizeof(router));
    ret->routes_str_pool = routes_pool;
    ret->types_str_pool = typename_pool;
    ret->entries = (r_array_t){.array = _entries, .chunk_size = sizeof(router_entry), .size = amount, .capacity = amount};
    ret->signature = ROUTER_SIGNATURE;
    return ret;
}

void router_destroy(router* r) {
    if (r->signature != ROUTER_SIGNATURE) return;
    VEC_FREE(r->routes_str_pool);
    VEC_FREE(r->types_str_pool);
    free(r->entries.array);
}
