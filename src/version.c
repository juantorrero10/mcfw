
#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <dlfcn.h>
#include <sys/inotify.h>
#include <assert.h>

#include <asclib/core.h>
#include <asclib/sync.h>

#include <crefl/core.h>
#include <crefl/scope.h>

#include "private/configs.h"
#include "private/version.h"
#include "private/log.h"
#include "private/router.h"

#define MAX(a, b) ((a) > (b))? (a) : (b)

#define INITIAL_CONTEXES_CAPACITY 4
#define APP_RELOAD_VER_INITIAL_VAL (uint32_t)-1

app_context_table APP_CTX_TABLE = {0};
uint32_t APP_CURR_VERSION = APP_RELOAD_VER_INITIAL_VAL;

async_(version_watch_async) {
    detach;
    pid_t parent_pid = getppid();
    char* watch_folder = (char*)g_app_config->app.source_folder;
    char* files[] = {watch_folder, NULL};

    while(1) {
        // Watch source files
        await(asc_inotify(files, NULL, 1, IN_MOVED_TO | IN_CLOSE_WRITE));

        // Send message to father
        kill(parent_pid, SIGUSR1);
        
        // Wait until compilation is finished.
        // If we detect a change in app .so -> trigger reload mechanism.
        // If the file is the "red herring" we ignore the change.
        char* file_changed;
        await(asc_inotify((char*[]){HOST_FOLDER, NULL}, &file_changed, 1, IN_MOVED_TO | IN_CLOSE_WRITE | IN_CREATE));
        if(!strcmp(file_changed, APP_SO)) {       
            load_app();
        } else if (!strcmp(file_changed, COMPILATION_RED_HERRING)){
            remove(COMPILATION_RED_HERRING);
        }
        free(file_changed);

    }
    end_return_void();
}


static void _append_context(app_context_table* t, app_context* ctx) {
    size_t found_idx = (size_t)-1;
    for (size_t i = 0; i < t->t.capacity; i++)
    {
        if (t->occupied[i] == 0) {found_idx = i; break;}
    }

    if (found_idx == (size_t)-1) {
        size_t old_c = t->t.capacity;
        t->t.capacity *= 2;
        t->t.array = realloc(t->t.array, t->t.chunk_size * t->t.capacity);
        t->occupied = realloc(t->occupied, t->t.capacity);
        memset(t->occupied + old_c, 0, old_c);
       
       
        found_idx = old_c;
    }

    assert(found_idx != (size_t)-1);
    memcpy((app_context*)t->t.array + found_idx, ctx, sizeof(app_context));
    t->occupied[found_idx] = 1;
    t->t.size++;
    
}

static inline void _free_context(app_context* c) {
    router_destroy(&c->router);
    dlclose(c->dl_handle);
}


static void _free_contexes_table() {
    for (size_t i = 0; i < APP_CTX_TABLE.t.capacity; i++)
    {
        if (APP_CTX_TABLE.occupied[i] != 1) continue;
        _free_context((app_context*)APP_CTX_TABLE.t.array + i);
    }
    
    free(APP_CTX_TABLE.t.array);
}

static void _register_context(router* r, void* dl_handle) {
    router rout = (r != NULL)? *r : (router){0};

    app_context c = {
        .router = rout,
        .dl_handle = dl_handle,
        .reload_version = APP_CURR_VERSION 
    };
    free(r);
    _append_context(&APP_CTX_TABLE, &c);
}

int load_app() {

    // Increase version
    APP_CURR_VERSION += 1;

    // Alocate table if is not set
    if (!APP_CTX_TABLE.t.array) {
        APP_CTX_TABLE.t = (r_array_t){
            .array = calloc(sizeof(app_context), INITIAL_CONTEXES_CAPACITY),
            .capacity = INITIAL_CONTEXES_CAPACITY,
            .size = 0,
            .chunk_size = sizeof(app_context)
        };
        APP_CTX_TABLE.occupied = malloc(INITIAL_CONTEXES_CAPACITY);
        memset(APP_CTX_TABLE.occupied, 0, INITIAL_CONTEXES_CAPACITY);
        atexit(_free_contexes_table);
    }

    // Hard link binary to a diferent name to get around dlopen() "lazyness"
    char unique_name[64];
    snprintf(unique_name, 64, APP_LIBS_PREFIX"_v%u.so", APP_CURR_VERSION);
    link(APP_SO, unique_name);

    void* app_handle = dlopen(unique_name, RTLD_NOW | RTLD_LOCAL);
    if (!app_handle) {
        mlog_error("Couldn't load application: %s", dlerror());
        unlink(unique_name);
        goto __exit_dont_apply;

    }
    unlink(unique_name);

    app_entry_fn entry_point = dlsym(app_handle, g_app_config->app.entry_point);
    if (!entry_point) {
        mlog_error("Couldn't find application entry point. %s", dlerror());
        goto __exit_dont_apply;

    }
    // Call app entry.
    router* r = entry_point();
    if (!r) {
        mlog_error("null pointer received from entry point");
        goto __exit_dont_apply;
    }

    if (r->signature != ROUTER_SIGNATURE) {
        mlog_error("pointer received from entry point (%p) was not a router ", r);
        goto __exit_dont_apply;
    }

    // Drop old version
    if (APP_CURR_VERSION - 1 != APP_RELOAD_VER_INITIAL_VAL) {
        // If there are no more clients, free the old context
        app_context* a = version_get_ctx(APP_CURR_VERSION - 1);
        if (a && a->ref_count == 0) version_drop_ctx(a);
    }

    _register_context(r, app_handle);
    return 1;

__exit_dont_apply:
    if (app_handle) dlclose(app_handle);
    APP_CURR_VERSION--; // Revert
    return 0;
}

app_context* version_get_ctx(uint32_t ver) {
    app_context* rte = NULL;
    for (size_t i = 0; i < APP_CTX_TABLE.t.capacity; i++)
    {
        if (APP_CTX_TABLE.occupied[i] == 0) continue;
        rte = (app_context*)APP_CTX_TABLE.t.array + i;
        if (rte->reload_version == ver) {
            return rte;
        }
    }
    return NULL;
}

/**
 * @brief Free an app context from the table. 
 */
void version_drop_ctx(app_context*  a) {
    // Assert that this is actually a pointer from the table, 
    assert(a >= (app_context*)APP_CTX_TABLE.t.array && a < (app_context*)APP_CTX_TABLE.t.array + APP_CTX_TABLE.t.capacity);
    
    size_t idx = (size_t)((void*)a - (void*)APP_CTX_TABLE.t.array);

    _free_context(a);

    // Mark as free;
    APP_CTX_TABLE.occupied[idx] = 0;

}