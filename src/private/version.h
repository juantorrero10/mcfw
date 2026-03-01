#ifndef PRIVATE_VERSION_H_
#define PRIVATE_VERSION_H_

#include "types.h"
#include "configs.h"
#include <asclib/core.h>

/**
 * @brief Free an app context from the table. 
 */
void version_drop_ctx(app_context*  a, int try_drop_scope);
decl_async(void, version_watch_async);
int load_app();
app_context* version_get_ctx(uint32_t ver);

#endif // PRIVATE_VERSION_H_