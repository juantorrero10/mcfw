
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include <crefl/core.h>
#include <crefl/types.h>
#include <crefl/serial/err.h>
#include <crefl/serial/formats.h>

#include "log.h"

#define SO_LOAD_CREFL "../dependencies/crefl/libcrefl.so"
#define APP_CONFIG_FILE "app_config.json"

typedef struct {
    uint32_t hr_enabled;
    uint32_t log_level;
} mcfw_arg_config;

REFLECT(struct) {
    uint32_t c_std;
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

app_config* g_app_config = NULL;
uint32_t reload_version = 0;

void mcfw_init(mcfw_arg_config* c) {
    refl_load_metadata(HOST_CREFL);
    refl_threadlocal_init();

    (void)c;
    
    RStruct* config_type = refl_fetch_struct("app_config");
    
    FILE* f = fopen(APP_CONFIG_FILE, "r");
    if (!f) {
        mlog_panic("mcfw_init", "Couldn't not open config file (%s): %s", APP_CONFIG_FILE, strerror(errno));
        _exit(1);
        
    }

    fseek(f, 0, SEEK_END);
    size_t cur = ftell(f);
    fseek(f, 0, SEEK_SET);
    void* config_contents = malloc(cur + 1);
    fread(config_contents, cur, 1, f);
    DeserializeResult dr = deserialize_json(config_type, config_contents);
    if (dr.st != DR_OK) {
        mlog_panic("mcfw_init", "Couldn't parse app configuration '%s'", dr.data.err_str);
        free(config_contents);
        _exit(1);
    }
    free(config_contents);
    g_app_config = dr.data.buff;
    mlog_info("mcfw_init", "Loaded config:");
    debug_print_struct(config_type, g_app_config, 1);

}