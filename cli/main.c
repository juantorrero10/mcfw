#include <stdio.h>
#include <string.h>

#include "vendor/argparse.h"
#include "commands.h"

int main(int argc, const char** argv) {
    int ll;
    
    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_GROUP("General options."),
        OPT_INTEGER('l', "log-level", &ll, "0 = INFO, 1 = WARNINGS, 2 = ERRORS AND PANICS", NULL, 0, 0),
        OPT_END(),
    };

    struct argparse a;

    argparse_init(&a, options, NULL, ARGPARSE_STOP_AT_NON_OPTION);
    argparse_describe(&a, 
        "\nMCFW: The Modern C Framework.", 
        "\nCommands: init, dev, build, daemon, clean, help");
    
    argc = argparse_parse(&a, argc, argv);

    // Figure command
    if (argc < 1) {
        fprintf(stderr, "No command given, see the list of commands ./mcfw --help\n");
        return 1;
    }
    char* cmd = (char*)argv[0];
    if (!strcmp(cmd, "init")) {
        cmd_init(); return 0;
    } else {
        fprintf(stderr, "Unknown command '%s', see the list of commands with --help\n", cmd);
        return 2;
    }
    

    return 0;
}