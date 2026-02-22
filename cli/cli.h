#ifndef COMMANDS_H_
#define COMMANDS_H_

extern char cache_string[];
extern int force_cache;

void cmd_init();
void cmd_build();

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

#define APP_CONFIG_TYPE "app_config"
#define APP_CONFIG_FILE "app_config.json"
#define CACHE_DIR "._ccomp_cache__"


#endif // COMMANDS_H_