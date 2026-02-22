#ifndef CONFIGS_H_
#define CONFIGS_H_

#include <crefl/types.h>

typedef struct {
    uint32_t hr_enabled;
    uint32_t log_level;
    uint32_t no_ss;
} mcfw_arg_config;

REFLECT(struct) {
    char* c_std;
    DYN_ARRAY(char*) cflags;
} __compilation_settings;

REFLECT(struct) {
    const char* source_folder;
    const char* entry_point;
    __compilation_settings compilation;
} __app_field;

REFLECT(struct) {
    uint8_t addr[4];
    uint16_t port;
}__host_field;

REFLECT(struct) {
    __app_field app;
    __host_field host;
} app_config;

#define APP_CONFIG_TYPE "app_config"
#define APP_CONFIG_FILE "app_config.json"
#define APP_SO          ".host/libapp.so"

extern app_config* g_app_config;

#endif // CONFIGS_H_