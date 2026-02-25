#define _GNU_SOURCE

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>

#include <crefl/core.h>
#include <crefl/serial/formats.h>

#include "private/types.h"
#include "private/log.h"
#include "private/router.h"


typedef struct {
    uint32_t reload_version;
    router router;
} router_table_entry;

typedef struct {
    DYN_ARRAY(router_table_entry) t;
} router_table;

router_table g_router_table;

#define INITIAL_ROUTER_TABLE_ENTRIES 4

#define ROUT_RET_ERR(_err) (routing_result){.data.err = (_err), .st = MCFW_ERR};
#define ROUT_RET_OK(_fptr) (routing_result){.data.method_fptr = (_fptr), .st = MCFW_OK};



static void _append_router(router_table* t, router* r) {
    if (t->t.size >= t->t.capacity) {
        t->t.array = realloc(t->t.array, t->t.chunk_size * 2 * t->t.capacity);
        t->t.capacity *= 2;
    }
    router_table_entry te = (router_table_entry){.reload_version = reload_version, .router = *r};
    free(r);
    memcpy((router_table_entry*)t->t.array + t->t.size++, &te, t->t.chunk_size);
}

static void _router_destroy(router* r) {
    for (size_t i = 0; i < r->entries.size; i++)
    {
        router_entry* re = (router_entry*)r->entries.array + i;
        free(re->route_str.data);   
    }
    free(r->entries.array);
}

static void _free_router_table() {
    for (size_t i = 0; i < g_router_table.t.size; i++)
    {
        router_table_entry* rte = (router_table_entry*)g_router_table.t.array + i;
        _router_destroy(&rte->router);
    }
    
    free(g_router_table.t.array);
}

void mcfw_register_router(router* r) {
    if (!r) {
        mlog_error("mcfw_register_router", "null ptr.");
        return;
    }
    if (r->signature != ROUTER_SIGNATURE) {
        mlog_error("mcfw_register_router", "pointer given: '%p' is garbage", r);
        return;
    }

    if (!g_router_table.t.array) {
        g_router_table.t = (r_array_t){
            .array = calloc(sizeof(router_table_entry), INITIAL_ROUTER_TABLE_ENTRIES),
            .capacity = INITIAL_ROUTER_TABLE_ENTRIES,
            .size = 0,
            .chunk_size = sizeof(router_table_entry)
        };
        atexit(_free_router_table);
    }
    _append_router(&g_router_table, r);
}

static inline router* _get_router_with_version(uint32_t ver) {
    uint32_t idx;
    for (size_t i = ver; i < g_router_table.t.size; i++)
    {
        idx = i % g_router_table.t.size;
        router_table_entry* rte = &((router_table_entry*)g_router_table.t.array)[idx];
        if (rte->reload_version == ver) {
            return &rte->router;
        }
    }
    return NULL;
}

static int _get_req_from_str(u8_vec* method) {
    REnum* e = refl_fetch_enum("request_kind");
    for (size_t i = 0; i < e->n_elements; i++)
    {
        size_t method_len = strlen(e->names[i]);
        if (method_len != method->count) continue;
        if (!memcmp(method->data, e->names[i], method->count)) return e->indexes[i];
    }
    
    return -1;
}

/**
 * @todo hash table
 */
routing_result mcfw_get_handler(u8_vec* method, u8_vec* route, uint32_t version) {
    mlog_info("mcfw_get_handler", "method: %.*s, route: %.*s", U8_VEC_FORMAT(*method), U8_VEC_FORMAT(*route));

    router* r = _get_router_with_version(version);
    if (!r) return ROUT_RET_ERR(HTTP_INTERNAL_SERVER_ERROR);

    for (size_t i = 0; i < r->entries.size; i++)
    {
        router_entry* re = (router_entry*)r->entries.array + i;
        int kind = _get_req_from_str(method);
        if (kind == -1) return ROUT_RET_ERR(HTTP_BAD_REQUEST);
        if (re->route_str.count != route->count) continue;
        if (!memcmp(re->route_str.data, route->data, re->route_str.count)) {
            if (re->r_kind != (request_kind)kind) return ROUT_RET_ERR(HTTP_METHOD_NOT_ALLOWED);
            return ROUT_RET_OK(re->method_fptr);
        }
    }
    return ROUT_RET_ERR(HTTP_NOT_FOUND);
}

router* router_create(router_entry* entries, size_t amount) {
    router_entry* _entries = calloc(sizeof(router_entry), amount);
    memcpy(_entries, entries, sizeof(router_entry) * amount);
    for (size_t i = 0; i < amount; i++)
    {
        _entries[i].route_str = entries[i].route_str;
        _entries[i].route_str.data = (uint8_t*)strdup((char*)entries[i].route_str.data);
    }
    
    router* ret = malloc(sizeof(router));
    ret->entries = (r_array_t){.array = _entries, .chunk_size = sizeof(router_entry), .size = amount, .capacity = amount};
    ret->signature = ROUTER_SIGNATURE;
    return ret;
}