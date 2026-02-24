#include <unistd.h>
#include <stdio.h>
#include <linux/limits.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <crefl/core.h>
#include <crefl/serial/err.h>
#include <crefl/serial/formats.h>

#include "vendor/argparse.h"
#include "private/configs.h"
#include "private/log.h"

#define SO_LOAD_CREFL "../dependencies/crefl/libcrefl.so"

mcfw_arg_config g_args = {0};
app_config* g_app_config = NULL;
uint32_t reload_version = 0;


static app_config* load_config();
static void __free_config();
static void parse_args(int argc, const char** argv);
void server_start();


int main(int argc, const char** argv) {

    parse_args(argc, argv);
    g_app_config = load_config();
    mlog_info("main", "loaded_config");
    atexit(__free_config);

    server_start();

    return 0;
}


void parse_args(int argc, const char** argv) {
    char* path = NULL;
    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_GROUP("General options"),
        OPT_STRING('p', "set-dir", &path, "set working directory", NULL, 0, 0),
        OPT_BOOLEAN('h', "enable-hot-reloading", &g_args.hr_enabled, "watch files in source folder for changes and reload the server.", NULL, 0, 0),
        OPT_INTEGER('l', "log-level", &g_args.log_level, "log-level.", NULL, 0, 0),
        OPT_BOOLEAN('s', "no-ss-handler", &g_args.no_ss,"disable asclib sigsegv handler", NULL, 0, 0),
        OPT_END()
    };

    struct argparse argparse;
    argparse_init(&argparse, options, NULL, 0);
    argparse_describe(&argparse, "\nServer host for the user code.", "\nThis program requieres a dynamic library that defines the app behavioue.");
    argparse_parse(&argparse, argc, argv);


    if (path) {
        if (chdir(path)) {
            exit(-1);
        }
        char p[PATH_MAX];
        char* shut_up_compiler = getcwd(p, PATH_MAX); (void)shut_up_compiler;
        printf("%s%spwd ==> %s%s\n",COLOR_GREEN, STYLE_BOLD, COLOR_RESET,p);
    }
}


static void __free_config() {
    refl_generic_destructor(refl_fetch_struct(APP_CONFIG_TYPE), g_app_config);
}

static app_config* load_config() {
    refl_load_metadata(HOST_CREFL);
    refl_threadlocal_init(1);

    
    RStruct* config_type = refl_fetch_struct(APP_CONFIG_TYPE);
    
    FILE* f = fopen(APP_CONFIG_FILE, "r");
    if (!f) {
        mlog_panic("mcfw_load_config", "Couldn't not open config file (%s): %s", APP_CONFIG_FILE, strerror(errno));
        _exit(1);
        
    }
    fseek(f, 0, SEEK_END);
    size_t cur = ftell(f);
    fseek(f, 0, SEEK_SET);

    void* config_contents = malloc(cur + 1);
    size_t sz = fread(config_contents, cur, 1, f);
    if (sz == 0) {
        mlog_panic("main", "Couldn't read app config %s", strerror(errno));
    }

    DeserializeResult dr = deserialize_json(config_type, config_contents);
    if (dr.st != DR_OK) {
        mlog_panic("mcfw_load_config", "Couldn't parse app configuration '%s'", dr.data.err_str);
        fclose(f);
        free(config_contents);
        _exit(1);
    }
    fclose(f);
    free(config_contents);
    refl_threadlocal_destroy();
    return dr.data.buff;
}