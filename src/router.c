#include <stdint.h>
#include <stdlib.h>

#include "types.h"
#include "log.h"

extern uint32_t reload_version;

typedef struct {
    uint32_t reload_version;
    router* router;
} router_table_entry;

typedef struct {
    router_table_entry* entries;
    size_t count;
    size_t capacity;
} router_table;

router_table g_router_table;

#define INITIAL_ROUTER_TABLE_ENTRIES 4


static void _append_router(router_table* t, router* r) {
    if (t->count >= t->capacity) {
        t->entries = realloc(t->entries, sizeof(router_entry) * 2 * t->capacity);
        t->capacity *= 2;
    }
    t->entries[t->count++] = (router_table_entry){.reload_version = reload_version, .router = r};
}

void mcfw_register_router(router* r) {
    if (r->signature != ROUTER_SIGNATURE) {
        mlog_error("mcfw_register_router", "pointer given: '%p' is garbage", r);
    }

    if (!g_router_table.entries) {
        g_router_table.entries = calloc(sizeof(router_table_entry), INITIAL_ROUTER_TABLE_ENTRIES);
    }
    _append_router(&g_router_table, r);
}