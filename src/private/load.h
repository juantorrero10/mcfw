#ifndef PRIVATE_LOAD_H_
#define PRIVATE_LOAD_H_

#include "types.h"
#include "configs.h"

typedef router* (*app_entry_fn)(void);

typedef struct {
    uint32_t reload_version;
    uint32_t ref_count;         // N of clients using the this router.
    void* dl_handle;
    void* crefl_handle;
    router router;
    uint8_t crefl_checksum[16];
} app_context;

typedef struct {
    // Occupation map to keep the contexes packed 
    uint8_t* occupied;
    DYN_ARRAY(app_context) t;
} app_context_table;

extern app_context_table g_app_context_table;
extern uint32_t g_curr_reload_version;

#define APP_CTX_TABLE g_app_context_table
#define APP_CURR_VERSION g_curr_reload_version

int load_app();
app_context* load_get_ctx_with_version(uint32_t ver);
/**
 * @brief Free an app context from the table. 
 */
void load_drop_ctx(app_context* a);

#endif // PRIVATE_RELOAD_H_