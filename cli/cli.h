#ifndef COMMANDS_H_
#define COMMANDS_H_

#include <stddef.h>

extern char cache_string[];
extern int force_cache;

int cmd_init();
int cmd_build();
int cmd_run();
int cmd_clean();

typedef struct {
    char* s;
    size_t cur;
    size_t capacity;
    size_t str_amnt;
} char_vec;

extern long mark_modified_timestamp;

void char_push(char_vec* v, char* s, size_t sz);
void cache_file_checksums(char* src_folder, int rec_lvl);
char** get_changed_files(char* src_folder, size_t* out_n_files);
int update_checksum(char* binary_file);

#define TERM_BY_SIGNAL (2 << 16)
int run_subprocess(char* file, char** argv, int daemon, char* redir_stdout, char* redir_stderr, int log);
int wait_parallel(size_t* n_jobs, size_t max_jobs);
void wait_all(size_t* n_jobs);
int wait_one(size_t* n_jobs);

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
    uint32_t serial_comp;
    uint32_t watch;
    uint32_t silent_comp;
} arg_flags;
extern arg_flags g_flags;



#define APP_CONFIG_TYPE "app_config"
#define APP_CONFIG_FILE "app_config.json"
#define CACHE_DIR "._ccomp_cache__"
#define CHECKSUM_FILE CACHE_DIR"/checksum"
#define COMPILATION_RED_HERRING "./.host/dont_reload"

#define COMPERR_REDIRECT "recomp-gcc.log"


#endif // COMMANDS_H_