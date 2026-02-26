#ifndef PRIVATE_VERSION_H_
#define PRIVATE_VERSION_H_

#include "types.h"
#include "configs.h"
#include <asclib/core.h>

typedef router* (*app_entry_fn)(void);

typedef struct {
    uint32_t reload_version;
    uint32_t ref_count;         // N of clients using the this router.
    void* dl_handle;
    void* crefl_handle;
    uint8_t crefl_checksum[16];
    router router;
} app_context;

typedef struct {
    // Occupation map to keep the contexes packed 
    uint8_t* occupied;
    DYN_ARRAY(app_context) t;
} app_context_table;

#define APP_CTX_TABLE g_app_context_table
#define APP_CURR_VERSION g_curr_reload_version

extern app_context_table APP_CTX_TABLE;
extern uint32_t APP_CURR_VERSION;

/**
 * @brief Free an app context from the table. 
 */
void version_drop_ctx(app_context* a);
decl_async(void, version_watch_async);
int load_app();
app_context* version_get_ctx(uint32_t ver);

#endif // PRIVATE_VERSION_H_