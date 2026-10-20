#define _XOPEN_SOURCE 500
#include <crefl/core.h>
#include <crefl/serial/err.h>
#include <crefl/serial/formats.h>
#include "cli.h"
#include "log.h"


#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>

#define MAX(a, b) ((a) > (b))? (a) : (b)


#define CC "gcc"
#define INCLUDE_FOLDER MCFW_DIR"/include"
#define DEPENDENCIES_FOLDER MCFW_DIR"/lib"
#define CREFL_GEN DEPENDENCIES_FOLDER"/crefl_gen"
#define ASCLIB_STATIC "libasclib.a"
#define TEMP_OUTPUT   CACHE_DIR"/__tempapp.so"
#define TARGET_SO     MCFW_DIR"/bin/libapp.so"
#define COMP_MARK     CACHE_DIR"/comp_mark"
#define PYTHON        "python3"

//                                              ⌄⌄ replaced by config
char* static_flags[] = {NULL, NULL, NULL,"-std=c00", "-I"INCLUDE_FOLDER, "-m64", "-MMD", "-MP", "-fPIC", "-o"};

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
        completed_chars, e, arrow, ">", MAX(30 - 1 - completed_chars, 0), s, (int)(percent * 100), cur_item);
    
}

static int reflect_types(char* sources_list, size_t total_sources) {
    // -> PY CREFL_GEN -i source_0... source_N -o out_file NULL
    char** argv = (char**)calloc(sizeof(char*), 1 + 1 + 1 + total_sources + 1 + 1 + 1);
    argv[0] = PYTHON;
    argv[1] = CREFL_GEN;
    argv[2] = "-i";
    argv[3 + total_sources] = "-o";
    argv[3 + total_sources + 1] = APP_CREFL_FILE;
    argv[3 + total_sources + 2] = NULL;
    char* p = sources_list;
    for (size_t i = 0; i < total_sources; i++)
    {
        argv[i + 3] = p;
        while(*p) p++;
        p++;
    }
    int ret = run_subprocess(PYTHON, argv, 0, "/dev/null", (g_flags.silent_comp)? COMPERR_REDIRECT : NULL, 0);
    free(argv);
    return ret == 0;
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

    int r = run_subprocess(CC, flags, 1, NULL, (g_flags.silent_comp)? COMPERR_REDIRECT : NULL, 0);
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

    int r = run_subprocess(CC, link_argv, 0, NULL, NULL, 0);
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

    // Create and truncate the log
    if (g_flags.silent_comp) close(creat(COMPERR_REDIRECT, 0644));

    app_config* c = g_app_config;

    // Compilation process
    char** cflags = NULL;
    char_vec obj_list = {0};
    char** changed_files;
    char* sources_list;
    size_t total_sources = 0;
    size_t n_changed = extract_sources_info((char*)c->app.source_folder, &changed_files, &sources_list, &total_sources);


    if (!n_changed) {
        if (!(recompilation && g_flags.silent_comp)) log_okay("Nothing to be done.");
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
    char* b = (char*)changed_files;
    cflags = get_comp_argv(c);
    size_t n_jobs = 0;
    // As many jobs as CPU cores
    size_t max_jobs = (g_flags.serial_comp)? 1 : sysconf(_SC_NPROCESSORS_ONLN);
    int last_exit_code = 0;

    // For the progress bar.
    char** processing_files = calloc(sizeof(char*), max_jobs);
    memset(processing_files, 0, sizeof(void*) * max_jobs);
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
            (recompilation && g_flags.silent_comp)
            ? log_error_nonl("Compilation failed")
            : log_fatal("One of the compilations failed.");
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
            (recompilation && g_flags.silent_comp)
            ? log_error_nonl("Compilation failed")
            : log_fatal("One of the compilations failed.");
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

    if (!reflect_types(sources_list, total_sources)) {
        free(sources_list);
        (recompilation && g_flags.silent_comp)
        ? log_error_nonl("Compilation failed.")
        : log_fatal("Couldn't generate the metadata of the types.");
        ret = 0;
        goto __exit_no_files;
    }
    free(sources_list);

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
        if (update_checksum(TEMP_OUTPUT))
            ret = 2; 
        printf("\033[K");
        fflush(stdout);
        // Clear the log and write a " " so you can use it as a errors screen 
        if (g_flags.silent_comp) {
            int fd = open(COMPERR_REDIRECT, O_CREAT | O_WRONLY | O_TRUNC, 0);
            ssize_t sz = write(fd, " ", 1);
            assert(sz >= 0);
            close(fd);
        }
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
    free((void*)changed_files);
    refl_threadlocal_destroy();
    return ret;
}