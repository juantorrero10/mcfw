#ifndef COMMANDS_H_
#define COMMANDS_H_

#include <stddef.h>

extern char cache_string[];
extern int force_cache;

int cmd_init();
int cmd_build();
int cmd_run();

typedef struct {
    char* s;
    size_t cur;
    size_t capacity;
    size_t str_amnt;
} char_vec;


void char_push(char_vec* v, char* s, size_t sz);
void cache_file_checksums(char* src_folder, int rec_lvl);
char** get_changed_files(char* src_folder, size_t* out_n_files);
void delete_cache_err(char** files, size_t n_files);


int run_subprocess(char* file, char** argv, int daemon, int quiet, int log);

#include <stdint.h>
#include <crefl/types.h>

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
extern app_config* g_app_config;

typedef struct {
    uint32_t force_compile;
    uint32_t log_level;
    uint32_t quiet;
    uint32_t daemon;
    uint32_t create_log;
} arg_flags;
extern arg_flags g_flags;



#define APP_CONFIG_TYPE "app_config"
#define APP_CONFIG_FILE "app_config.json"
#define CACHE_DIR "._ccomp_cache__"


#endif // COMMANDS_H_