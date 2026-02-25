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
#include <dirent.h>
#include <fcntl.h>
#include <time.h>


#define CC "gcc"
#define DEPENDENCIES_FOLDER "../dependencies"
#define ASCLIB_STATIC "asclib/libasclib.a"
#define TEMP_OUTPUT   CACHE_DIR"/__tempapp.so"
#define TARGET_SO     ".host/libapp.so"
#define COMP_MARK     CACHE_DIR"/comp_mark"

//                                              ⌄⌄ replaced by config
char* static_flags[] = {NULL, NULL, NULL,"-std=c00", "-I"DEPENDENCIES_FOLDER, "-m64", "-MMD", "-MP", "-fPIC", "-o"};

#define STATIC_AMNT sizeof(static_flags) / sizeof(char*)

char* link_static_flags[] = {"-fPIC", "-shared",
    "-Wl,-Bsymbolic", "-o", TEMP_OUTPUT};

#define LINK_STATIC_AMNT sizeof(link_static_flags) / sizeof(char*)

long mark_modified_timestamp = -1;



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

static int compile_obj(char* file, char** flags) {
    flags[2] = file;
    char* out_file = strdup(file);
    char* ptr = out_file;
    while((ptr = strchr(ptr, '/')) != NULL) {
        *ptr = '_';
    }
    size_t sz = strlen(out_file);
    out_file[sz - 1] = 'o';

    char buff[4096];
    sprintf(buff, "./"CACHE_DIR"/obj/%s", out_file);
    flags[STATIC_AMNT] = buff;

    int r = run_subprocess(CC, flags, 1, 0, 0);
    free(out_file);
    flags[STATIC_AMNT] = NULL;
    return r;
}

static inline void make_obj_dir() {
    struct stat st = {0};
    if (stat(CACHE_DIR"/obj", &st) == -1)
        mkdir(CACHE_DIR"/obj", 0700);
}

static int link_objects(char* obj_list, size_t n_objects) {
    char** link_argv = calloc(sizeof(char*), 1 + n_objects + LINK_STATIC_AMNT + 1);
    link_argv[0] = CC;
    char* file = obj_list;
    for (size_t i = 0; i < n_objects; i++)
    {
        link_argv[i + 1] = file;
        while(*file) 
        { 
            file++; 
        }
        file++;
    }
    memcpy(link_argv + 1 + n_objects, link_static_flags, sizeof(char*) * LINK_STATIC_AMNT);
    link_argv[1 + n_objects + LINK_STATIC_AMNT] = NULL;
    

    prog_bar p = {.op_name = "Linking", .n_tasks = 1, .completed = 0};
    render_progress_bar(&p, TARGET_SO);

    int r = run_subprocess(CC, link_argv, 0, 0, 0);
    free(link_argv);
    if (r != 0) return 0;
    p.completed++;
    render_progress_bar(&p, "done");
    return 1;
}



static char** get_comp_argv(app_config* c) {
    size_t n = STATIC_AMNT + c->app.compilation.cflags.size + 1;
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
            ret[i] = strdup(((char**)c->app.compilation.cflags.array)[i - STATIC_AMNT - 1]);
    }
    ret[n] = NULL;
    return ret;
}


static void get_obj_list(char_vec* v) {
    DIR* d = opendir(CACHE_DIR"/obj");
    if (!d) {
        log_fatal("Could not open obj directory: "CACHE_DIR"/obj");
        _Exit(1);
    }
    struct dirent* entry;
    while((entry = readdir(d)) != NULL) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;
    
        size_t sz = strlen(entry->d_name);
        // Wrong extension
        if (memcmp(entry->d_name + sz - 2, ".o", 2) != 0) continue;
        char_push(v, CACHE_DIR"/obj/", sizeof(CACHE_DIR) -1 + 5);
        char_push(v, entry->d_name, sz);
        char_push(v, "\0", 1);
        v->str_amnt++;
    }
    closedir(d);
}




int cmd_build(int recompilation) {
    int ret = 1;

    struct stat st;
    int e = stat(DEPENDENCIES_FOLDER, &st) == 0;
    if (!e || !S_ISDIR(st.st_mode)) {
        log_fatal("Dependencies folder not present, make sure the framework is compiled correctly.");
        _Exit(1);
    }

    // Get timestamp of .so file, (-1) if not created
    e = stat(COMP_MARK, &st) == 0;
    if (e && !S_ISDIR(st.st_mode)) {
        mark_modified_timestamp = st.st_mtime;
    }

    app_config* c = g_app_config;

    // Compilation process
    size_t n_changed;
    char** cflags = NULL;
    char_vec obj_list = {0};

    char** files = get_changed_files((char*)c->app.source_folder, &n_changed);

    if (!n_changed) {
        log_okay("Nothing to be done.");
        // Check if output exists because the linkage might've failed.
        // Also check if last compilation happened later than last linkage
        e = stat(TARGET_SO, &st) == 0;
        if (!e || S_ISDIR(st.st_mode) || mark_modified_timestamp == -1 || mark_modified_timestamp > st.st_mtime) {
            // Create a file of the commnds run on this compilation
            close(creat(CACHE_DIR"/last_comp.txt", 0644));

            goto __link;
        }
        ret = 2;
        goto __exit_no_files;
    }

    // Create a file of the commnds run on this compilation
    close(creat(CACHE_DIR"/last_comp.txt", 0644));
    
    
    prog_bar p = {.op_name = "Compiling", .n_tasks = n_changed, .completed = 0};
    make_obj_dir();
    char* b = (char*)files;
    cflags = get_comp_argv(c);
    size_t n_jobs = 0;
    // As many jobs as CPU cores
    size_t max_jobs = (g_flags.serial_comp)? 1 : sysconf(_SC_NPROCESSORS_ONLN);
    int last_exit_code = 0;

    // For the progress bar.
    char** processing_files = calloc(sizeof(char*), max_jobs);
    pid_t* pid_pool = calloc(sizeof(pid_t), max_jobs);
    memset(pid_pool, 0, sizeof(pid_t) * max_jobs);
    size_t idx;

    // Launch N compilations
    for (size_t i = 0; i < n_changed; i++)
    {
        // Initial render
        render_progress_bar(&p, b);
        // Launch compilation job
        pid_t pid = compile_obj(b, cflags);
        
        // Register name and pid on first available spot
        idx = 0;
        while(pid_pool[idx] != 0) idx++;
        pid_pool[idx] = pid;
        processing_files[idx] = b;

        n_jobs++;
        last_exit_code = wait_parallel(&n_jobs, max_jobs); 
        if (!last_exit_code) {
            wait_all(&n_jobs);
            log_fatal("One of the compilations failed.");
            ret = 0;
            goto __exit;
        };
        if (last_exit_code > 0) {
            p.completed++;
            // find idx
            idx = 0;
            while(pid_pool[idx] != last_exit_code) idx++;
            pid_pool[idx] = 0;

            // Render one that isn't free
            while(pid_pool[idx] != 0) idx = (idx + 1) % max_jobs;
            render_progress_bar(&p, processing_files[idx]);
        }
        while(*b) b++;
        b++;
    }
    while (n_jobs != 0) {
        pid_t pid;
        if (!(pid = wait_one(&n_jobs))) {
            wait_all(&n_jobs);
            log_fatal("One of the compilations failed.");
            ret = 0;
            goto __exit;   
        }
        if (pid < 0) continue;
        
        // Find our spot
        idx = 0;
        while(pid_pool[idx] != pid) idx++;
        pid_pool[idx] = 0;
        // Render one that isn't free
        while(pid_pool[idx] != 0) idx = (idx + 1) % max_jobs;
        p.completed++;
        render_progress_bar(&p, processing_files[idx]);
    }

    // if compilation succeds, save an file to mark the timestamp of last comp
    close(creat(COMP_MARK, 0644));
    if (!recompilation) log_okay("Compilation succeded.");

    // Link
__link:
    obj_list = (char_vec){.s = calloc(1, 50), .capacity = 50, .cur = 0};
    get_obj_list(&obj_list);
    if (!link_objects(obj_list.s, obj_list.str_amnt)) {
        log_fatal("Linking failed.");
        ret = 0;
        goto __exit;
    }
    if (recompilation) {
        if (update_checksum(TEMP_OUTPUT)) {
            log_okay("Output did not change."); 
            ret = 2;
        } else log_okay("Recompiled.");
    } else {
        log_okay("Linking succesful.");
    }
    // Commit to final output
    if (ret == 2) {
        remove(TEMP_OUTPUT); 
        goto __exit; 
    }
    int n = rename(TEMP_OUTPUT, TARGET_SO);
    if (n != 0) {
        log_fatal("Couln't produce output: %s", strerror(errno));
    }
    if (!recompilation) printf(COLOR_WHITE""STYLE_BOLD"==> "TARGET_SO""COLOR_RESET"\n");

__exit:
    if (cflags) {
        for (size_t i = 3; i < STATIC_AMNT + c->app.compilation.cflags.size + 1; i++) 
        { 
            free(cflags[i]); 
        }
        free(cflags);
    }
    free(obj_list.s);
    free(pid_pool);
    free(processing_files);
__exit_no_files:
    free((void*)files);
    refl_threadlocal_destroy();
    return ret;
}