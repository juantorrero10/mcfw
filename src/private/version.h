#ifndef PRIVATE_VERSION_H_
#define PRIVATE_VERSION_H_

#include "types.h"
#include "configs.h"
#include <asclib/core.h>

#define APP_CTX_TABLE g_app_context_table
#define CREFL_SCOPES_TABLE g_crefl_scopes_table
#define APP_CURR_VERSION g_curr_reload_version

extern app_context_table APP_CTX_TABLE;
extern crefl_scope_table CREFL_SCOPES_TABLE;
extern uint32_t APP_CURR_VERSION;

/**
 * @brief Free an app context from the table. 
 */
void version_drop_ctx(app_context*  a, int try_drop_scope);
decl_async(void, version_watch_async);
int load_app();
app_context* version_get_ctx(uint32_t ver);

#endif // PRIVATE_VERSION_H_