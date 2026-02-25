#ifndef PRIVATE_LOAD_H_
#define PRIVATE_LOAD_H_

#include "types.h"
#include "configs.h"

typedef router* (*app_entry_fn)(void);

typedef struct {
    uint32_t reload_version;
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

int load_app(app_config* config);
app_context* load_get_ctx_with_version(uint32_t ver);

#endif // PRIVATE_RELOAD_H_