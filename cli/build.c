#define _XOPEN_SOURCE 500
#include <crefl/core.h>
#include <crefl/serial/err.h>
#include <crefl/serial/formats.h>
#include "cli.h"
#include "log.h"


#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>


#define CC "gcc"
#define DEPENDENCIES_FOLDER "../dependencies"
char* static_flags[] = {NULL, NULL, NULL,"-std=c00", "-I"DEPENDENCIES_FOLDER, "-L"DEPENDENCIES_FOLDER, "-fPIC", "-shared", "-o"}; 
#define STATIC_AMNT 9

typedef struct {
    char* op_name;
    size_t n_tasks;
    size_t completed;
} prog_bar;

static void render_progress_bar(prog_bar* b, char* cur_item) {
    // [                             ]
    char* s = "                              ";
    char* e = "==============================";
    double percent = (double)b->completed / b->n_tasks;
    int completed_chars = (int)(30 * percent);
    int arrow = b->completed < b->n_tasks;
    log_inprogress(b->op_name, "[%.*s%.*s%.*s] %d%%   %s",
        completed_chars, e, arrow, ">", 30 - 1 - completed_chars, s, (int)(percent * 100), cur_item);
    
}

static int compile_obj(char* file, prog_bar* p, char** flags) {
    flags[2] = file;
    char* out_file = strdup(file);
    char* ptr = out_file;
    while((ptr = strchr(ptr, '/')) != NULL) {
        *ptr = '_';
    }
    size_t sz = strlen(out_file);
    out_file[sz - 1] = 'o';

    render_progress_bar(p, file);
    char buff[4096];
    sprintf(buff, "./"CACHE_DIR"/obj/%s", out_file);
    flags[STATIC_AMNT] = buff;

    int ret = run_subprocess(CC, flags, 0, 0, 0);
    free(out_file);
    flags[STATIC_AMNT] = NULL;
    if (ret != 0) return 0;
    p->completed++;
    return 1;
}

static inline void make_obj_dir() {
    struct stat st = {0};
    if (stat(CACHE_DIR"/obj", &st) == -1)
        mkdir(CACHE_DIR"/obj", 0700);
}


static char** get_comp_argv(app_config* c) {
    size_t n = STATIC_AMNT + c->app.compilation.cflags.size;
    char** ret = calloc(sizeof(char*), n + 1);
    // idx 0 is the compiler
    ret[0] = CC;
    // idx 1 -c flag
    ret[1] = "-c";
    // idx 2 input file (skip for now)
    // idx 3 the c std (special case)
    char* std_flag = strdup(static_flags[3]);
    memcpy(std_flag + 6, c->app.compilation.c_std, 2);
    ret[3] = std_flag;

    for (size_t i = 4; i < n; i++)
    {
        if (i < STATIC_AMNT) {
            ret[i] = strdup(static_flags[i]);
        } else if (i == STATIC_AMNT) continue;
        else if (c->app.compilation.cflags.size > 0)
            ret[i] = strdup(((char**)c->app.compilation.cflags.array)[i - STATIC_AMNT]);
    }
    ret[n] = NULL;
    return ret;
}


void cmd_build() {
    RStruct* config_type = refl_fetch_struct(APP_CONFIG_TYPE);
    FILE* f = fopen(APP_CONFIG_FILE, "r");
    if (!f) {
        log_fatal("Couldn't open config file (%s): %s", APP_CONFIG_FILE, strerror(errno));
        _Exit(1);
        
    }
    struct stat st;
    int e = stat(DEPENDENCIES_FOLDER, &st) == 0;
    if (!e || !S_ISDIR(st.st_mode)) {
        log_fatal("Dependencies folder not present, make sure the framework is compiled correctly.");
        _Exit(1);
    }

    fseek(f, 0, SEEK_END);
    size_t cur = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* j = malloc(cur + 1);
    fread(j, cur, 1, f);
    fclose(f);

    DeserializeResult dr = deserialize_json(config_type, j);
    free(j);
    if (dr.st != DR_OK) {
        log_fatal("Couldn't parse config file: %s", dr.data.err_str);
        _Exit(2);
    }

    app_config* c = dr.data.buff;
    //cache_file_checksums((char*)c->app.source_folder, 0);
    size_t n_changed;
    char** files = get_changed_files((char*)c->app.source_folder, &n_changed);
    if (!n_changed) {
        log_okay("No source files had changed since last compilation.");
        goto __exit_no_files;
    }
    creat(CACHE_DIR"/last_comp.cmd", 0644);

    
    prog_bar p = {.op_name = "Compiling", .n_tasks = n_changed, .completed = 0};
    make_obj_dir();
    char* b = (char*)files;
    char** cflags = get_comp_argv(c);
    for (size_t i = 0; i < n_changed; i++)
    {
        if (!compile_obj(b, &p, cflags)) {
            log_fatal("One of the compilations failed.");
            delete_cache_err(files, n_changed);
            goto __exit;
        };
        while(*b) b++;
        b++;
    }
    log_okay("Compilation succeded.");
    
__exit:
    for (size_t i = 3; i < STATIC_AMNT + c->app.compilation.cflags.size; i++)
    {
        free(cflags[i]);
    }
    free(cflags);
__exit_no_files:
    free((void*)files);
    refl_generic_destructor(config_type, c);
}