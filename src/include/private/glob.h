#ifndef PRIVATE_GLOB_H_
#define PRIVATE_GLOB_H_

#include <private/http.h>
#include <private/version.h>
#include <crefl/types.h>

#define CACHED_ENUM_STR_STATUS g_cached_str_status
#define CACHED_ENUM_REQ_KIND g_cached_request_kind

extern const REnum* CACHED_ENUM_STR_STATUS;
extern const REnum* CACHED_ENUM_REQ_KIND;

#define ARG_FLAGS g_args
#define APP_CONFIG_STRUCT g_app_config

extern mcfw_arg_config ARG_FLAGS;
extern app_config* APP_CONFIG_STRUCT;


#define APP_CTX_TABLE g_app_context_table
#define CREFL_SCOPES_TABLE g_crefl_scopes_table
#define APP_CURR_VERSION g_curr_reload_version

extern AppContextTable APP_CTX_TABLE;
extern CReflScopeTable CREFL_SCOPES_TABLE;
extern uint32_t APP_CURR_VERSION;

#define STR_SCRATCH g_str_scratch

extern char STR_SCRATCH[4096];

extern const int req_deserializing_lut[];
extern const int req_serializing_lut[];
extern char* str_response_type_lut[];

extern MIMELutEntry MIME_LUT[];

#endif // PRIVATE_GLOB_H_