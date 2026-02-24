#define _GNU_SOURCE

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>

#include <crefl/core.h>
#include <crefl/serial/formats.h>

#include "public/types.h"
#include "log.h"

extern uint32_t reload_version;

REFLECT(struct) {
    uint32_t reload_version;
    router router;
} router_table_entry;

REFLECT(struct) {
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
    memcpy((router_table_entry*)t->t.array + t->t.size++, &te, t->t.chunk_size);
}

static void _free_router_table() {free(g_router_table.t.array);}

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
    debug_print_struct(refl_fetch_struct("router_table"), &g_router_table, 1);
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

static int _get_req_from_str(char* method) {
    REnum* e = refl_fetch_enum("request_kind");
    for (size_t i = 0; i < e->n_elements; i++)
    {
        if (!strcmp(method, e->names[i])) return e->indexes[i];
    }
    
    return -1;
}

/**
 * @todo hash table
 */
routing_result mcfw_handler(char* method, char* route, uint32_t version) {
    router* r = _get_router_with_version(version);
    mlog_info("mcfw_handler", "%s - %s", method, route);
    if (!r) return ROUT_RET_ERR(HTTP_INTERNAL_SERVER_ERROR);

    for (size_t i = 0; i < r->entries.size; i++)
    {
        router_entry* re = (router_entry*)r->entries.array + i;
        debug_print_struct(refl_fetch_struct("router_entry"), re, 1);
        int kind = _get_req_from_str(method);
        if (kind == -1) return ROUT_RET_ERR(HTTP_BAD_REQUEST);
        mlog_info("mcfw_handler", "%s vs %s", re->route, route);
        if (!strcmp(re->route, route)) {
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
        _entries[i].route = strdup(entries[i].route);
    }
    
    router* ret = malloc(sizeof(router));
    ret->entries = (r_array_t){.array = _entries, .chunk_size = sizeof(router_entry), .size = amount, .capacity = amount};
    ret->signature = ROUTER_SIGNATURE;
    return ret;
}