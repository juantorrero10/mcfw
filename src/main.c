#include <unistd.h>
#include <stdio.h>
#include <linux/limits.h>
#include "vendor/argparse.h"
#include <crefl/core.h>

int g_hr_enabled = 0;



void parse_args(int argc, const char** argv) {
    char* path = NULL;
    int eht = 0;
    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_STRING('p', "set-dir", &path, "set working directory", NULL, 0, 0),
        OPT_BOOLEAN('h', "enable-hot-reloading", &eht, "watch files in source folder for changes and reload the server.", NULL, 0, 0),
        OPT_END()
    };

    struct argparse argparse;
    argparse_init(&argparse, options, NULL, 0);
    argparse_describe(&argparse, "\nServer host for the user code.", "\nThis program requieres a dynamic library that defines the app behavioue.");
    argparse_parse(&argparse, argc, argv);


    if (path) {
        chdir(path);
    }
    g_hr_enabled = eht;
}


int main(int argc, const char** argv) {

    parse_args(argc, argv);
    

    char cwd[PATH_MAX];
    getcwd(cwd, PATH_MAX);
    printf("cwd: %s\n", cwd);
    return 0;
}