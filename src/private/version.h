#ifndef PRIVATE_VERSION_H_
#define PRIVATE_VERSION_H_

#include "types.h"
#include "configs.h"
#include "router.h"
#include <asclib/core.h>

typedef struct {
    void* c_handle;
    size_t c_ctx_ref_count; // N of ctxes using this version.
    size_t at_index;        // Position on the array of pointers
    uint16_t c_checksum[16];
} crefl_scope;


typedef struct {
    uint32_t reload_version;
    uint32_t ref_count;         // N of clients using the this router.
    void* dl_handle;
    size_t at_index;        // Position on the array of pointers
    crefl_scope* crefl_data;
    router router;
} app_context;

typedef struct {
    // Occupation map to keep the contexes packed 
    uint8_t* occupied;
    DYN_ARRAY(app_context*) t;
} app_context_table;

typedef struct {
    uint8_t* occupied;
    DYN_ARRAY(crefl_scope*) t;
} crefl_scope_table;

/**
 * @brief Free an app context from the table.
 * @returns 1 or 0 depending if it has dropped the scope or not 
 */
int version_drop_ctx(app_context*  a, int try_drop_scope);
decl_async(void, version_watch_async);
int load_app();
app_context* version_get_ctx(uint32_t ver);

#endif // PRIVATE_VERSION_H_