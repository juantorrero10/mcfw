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

#define ROUT_RET_ERR(_err) (routing_result){.data.err = (_err), .st = MCFW_ERR};
#define ROUT_RET_OK(_fptr) (routing_result){.data.method_fptr = (_fptr), .st = MCFW_OK};





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
routing_result router_get_handler(router* r, u8_vec* method, u8_vec* route) {

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

void router_destroy(router* r) {
    if (r->signature != ROUTER_SIGNATURE) return;
    for (size_t i = 0; i < r->entries.size; i++)
    {
        router_entry* re = (router_entry*)r->entries.array + i;
        free(re->route_str.data);   
    }
    free(r->entries.array);
}
