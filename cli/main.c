#include <stdio.h>
#include <string.h>

#include <crefl/types.h>
#include <crefl/core.h>

#include "vendor/argparse.h"
#include "cli.h"
#include "log.h"

char cache_string[4096];

int force_cache = 0;

typedef void(*command)(void);

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
    } else if (!strcmp(cmd, "--help")){
        *help = 1;
        return NULL;
    } else {
        log_fatal("Unknown command '%s', see the list of commands with --help", cmd);
        return NULL;
    }
}

int main(int argc, const char** argv) {
    int ll;

    // Command
    int help;
    command fptr = load_command_ptr(argc, argv, &help);
    if (!fptr && !help) return 1;
    
    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_GROUP("General options."),
        OPT_INTEGER('l', "log-level", &ll, "0 = INFO, 1 = WARNINGS, 2 = ERRORS AND PANICS", NULL, 0, 0),
        OPT_BOOLEAN('f', "force-compile", &force_cache, "Compile all files regardless if they haven't changed", NULL, 0, 0),
        OPT_END(),
    };

    struct argparse a;

    argparse_init(&a, options, NULL, ARGPARSE_STOP_AT_NON_OPTION);
    argparse_describe(&a, 
        "\nMCFW: The Modern C Framework.", 
        "\nCommands: init, dev, build, daemon, clean, help");
    
    argparse_parse(&a, (help)? argc : argc - 1, (help)? argv : argv + 1);

    refl_load_metadata(CLI_CREFL);
    refl_threadlocal_init(0);

    fptr();
    

    return 0;
}