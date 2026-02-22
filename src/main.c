#include <unistd.h>
#include <stdio.h>
#include <linux/limits.h>
#include "vendor/argparse.h"
#include <crefl/core.h>

typedef struct {
    uint32_t hr_enabled;
    uint32_t log_level;
} mcfw_arg_config;


void parse_args(int argc, const char** argv, mcfw_arg_config* c) {
    char* path = NULL;
    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_STRING('p', "set-dir", &path, "set working directory", NULL, 0, 0),
        OPT_BOOLEAN('h', "enable-hot-reloading", &c->hr_enabled, "watch files in source folder for changes and reload the server.", NULL, 0, 0),
        OPT_INTEGER('l', "log-level", &c->log_level, "log-level.", NULL, 0, 0),
        OPT_END()
    };

    struct argparse argparse;
    argparse_init(&argparse, options, NULL, 0);
    argparse_describe(&argparse, "\nServer host for the user code.", "\nThis program requieres a dynamic library that defines the app behavioue.");
    argparse_parse(&argparse, argc, argv);


    if (path) {
        chdir(path);
    }
}

void mcfw_init(mcfw_arg_config* c);


int main(int argc, const char** argv) {

    mcfw_arg_config c = {0};
    parse_args(argc, argv, &c);
    mcfw_init(&c);
    return 0;
}