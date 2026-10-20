#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#include <crefl/types.h>
#include <crefl/core.h>
#include <crefl/serial/err.h>
#include <crefl/serial/formats.h>

#include "vendor/argparse.h"
#include "cli.h"
#include "log.h"

char cache_string[4096];

app_config* g_app_config = NULL;
arg_flags g_flags = {0};

typedef int(*command)(int);

static command load_command_ptr(int argc, const char** argv, int* help) {
    if (argc < 2) {
        log_warn("No command given, see the list of commands ./mcfw --help");
        return NULL; 
    }
    char* cmd = (char*)argv[1];
    if (!strcmp(cmd, "init")) {
        return cmd_init;
    } else if (!strcmp(cmd, "build")) {
        return cmd_build;
    } else if (!strcmp(cmd, "run")){
        return cmd_run;
    } else if (!strcmp(cmd, "watch")) {
        g_flags.watch = 1;
        return cmd_run;
    } else if (!strcmp(cmd, "clean")) {
        return cmd_clean;
    } else if (!strcmp(cmd, "--help")){
        *help = 1;
        return NULL;
    } else {
        log_fatal("Unknown command '%s', see the list of commands with --help", cmd);
        return NULL;
    }
}

int main(int argc, const char** argv) {

    // Command
    int help = 0;
    command fptr = load_command_ptr(argc, argv, &help);
    if (!fptr && !help) return 1;

    char* p_ptr = NULL;
    
    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_GROUP("Compilation options."),
        OPT_BOOLEAN('f', "force-compile", &g_flags.force_compile, "Ignore cached obj files, recompile everything", NULL, 0, 0),
        OPT_BOOLEAN('s', "serial-compilation", &g_flags.serial_comp ,"Do not parallelize the compilation.", NULL, 0, 0),
        OPT_BOOLEAN(0, "silent-comp", &g_flags.silent_comp, "Redirect compilarion gcc errors to \""COMPERR_REDIRECT"\".", NULL, 0, 0),
        OPT_GROUP("Runtime options."),
        OPT_INTEGER(0, "log-level", &g_flags.log_level, "0 = INFO, 1 = WARNINGS, 2 = ERRORS AND PANICS", NULL, 0, 0),
        OPT_BOOLEAN('q', "quiet", &g_flags.quiet, "Run without output.", NULL, 0, 0),
        OPT_BOOLEAN('d', "daemon", &g_flags.daemon, "Run in background", NULL, 0, 0),
        OPT_GROUP("Misc."),
        OPT_STRING(0, "set-pwd", &p_ptr, "Set working directory.", NULL, 0, 0),
        OPT_END(),
    };



    struct argparse a;

    argparse_init(&a, options, NULL, ARGPARSE_STOP_AT_NON_OPTION);
    argparse_describe(&a, 
        "\nMCFW: The Modern C Framework.", 
        "\nCommands: init, dev, build, daemon, clean, help");
    
    argparse_parse(&a, (help)? argc : argc - 1, (help)? argv : argv + 1);


    if (p_ptr) {
        if (chdir(p_ptr) != 0) {
            log_fatal("Couldn't set pwd: %s", strerror(errno));
            _exit(1);
        }
    }

    if (g_flags.daemon && g_flags.watch) {
        log_fatal("'watch' command is incompatible with --daemon (-d) flag.");
        _exit(1);
    }

    if (fptr == cmd_init) {
        return cmd_init();
    }

    refl_load_metadata(CLI_CREFL);
    refl_threadlocal_init(1);

    RStruct* config_type = refl_fetch_struct(APP_CONFIG_TYPE);
    FILE* f = fopen(APP_CONFIG_FILE, "r");
    if (!f) {
        log_fatal("Couldn't open config file (%s): %s", APP_CONFIG_FILE, strerror(errno));
        _Exit(1);
        
    }


    fseek(f, 0, SEEK_END);
    size_t cur = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* j = malloc(cur + 1);
    ssize_t sz = fread(j, cur, 1, f); (void)sz;
    fclose(f);

    DeserializeResult dr = deserialize_json(config_type, j, cur);
    free(j);
    if (dr.st != DR_OK) {
        log_fatal("Couldn't parse config file: %s", dr.data.err_str);
        _Exit(2);
    }

    g_app_config = dr.data.buff;

    fptr(0);
    if (g_app_config) refl_generic_destructor(refl_fetch_struct(APP_CONFIG_TYPE), g_app_config);

    return 0;
}