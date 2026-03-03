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
#define HOST_FOLDER     ".host"
#define APP_LIBS_PREFIX          HOST_FOLDER"/libapp"
#define APP_SO                   APP_LIBS_PREFIX".so"
#define COMPILATION_RED_HERRING HOST_FOLDER"/dont_reload"
#define CREFL_FOLDER    ".crefl"
#define CREFL_APP_FILE CREFL_FOLDER"/app_types.crefl"

#define SERVER_HEADER_VAL "mcfw-dev (unstable)"

#define MAX_HEADER_SIZE  8192     // 8kb
#define MAX_BODY_SIZE 1024 * 1024 // 1 Mb
// Needs to be a power of 2
#define PHR_HEADER_BUFF_SIZE 64

#endif // CONFIGS_H_