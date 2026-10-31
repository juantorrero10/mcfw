#ifndef CONFIGS_H_
#define CONFIGS_H_

#include <crefl/types.h>

typedef struct {
    u32 hr_enabled;
    u32 log_level;
    u32 no_ss;
} mcfw_arg_config;

REFLECT(struct) {
    strZ c_std;
    DYN_ARRAY(strZ) cflags;
} __compilation_settings;

REFLECT(struct) {
    strZ source_folder;
    strZ entry_point;
    __compilation_settings compilation;
} __app_field;

REFLECT(struct) {
    u8 addr[4];
    u16 port;
}__host_field;

REFLECT(struct) {
    __app_field app;
    __host_field host;
} app_config;

#define APP_CONFIG_TYPE "app_config"
#define APP_CONFIG_FILE "app_config.json"
#define MCFW_WDIR       ".mcfw"
#define HOST_FOLDER     MCFW_WDIR"/bin"
#define APP_LIBS_PREFIX          HOST_FOLDER"/libapp"
#define APP_SO                   APP_LIBS_PREFIX".so"
#define COMPILATION_RED_HERRING HOST_FOLDER"/dont_reload"
#define CREFL_FOLDER    MCFW_WDIR"/crefl"
#define CREFL_APP_FILE CREFL_FOLDER"/app_types.crefl"

#define SERVER_HEADER_VAL "mcfw-"MCFW_VERSION_TAG" "MCFW_VERSION

#define MAX_HEADERS_SIZE  16 * 1024     // 16KB
#define RECV_ATT_SIZE 8 * 1024          // 8KB
#define RECV_BUFF_SIZE 32 * 1024        // 32KB 
#define MAX_BODY_SIZE 1024 * 1024       // 1 MB

#define TOTAL_HEADER_TIMEOUT 5000
#define RECV_HEADER_TIMEOUT 2000

#define TOTAL_BODY_TIMEOUT 30000
#define RECV_BODY_TIMEOUT 5000

#define SEND_TIMEOUT 2000

#define MAX_REQ_PER_CONNECTION 100

// Needs to be a power of 2
#define MAX_REQ_HEADER_COUNT 128
#define MAX_RES_HEADER_COUNT 64
#define HEADER_HASHTABLE_CAPACITY 2 * MAX_REQ_HEADER_COUNT // At worst 50% occupation

#endif // CONFIGS_H_