
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
#include "private/load.h"
#include "private/log.h"
#include "private/router.h"



decl_async(void, hot_reload_watch);

#define INITIAL_CONTEXES_CAPACITY 4

app_context_table g_app_context_table = {0};

uint32_t g_curr_reload_version = (uint32_t)-1;

async_(hot_reload_watch) {
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
            /** @todo actually reload */ 
            mlog_info("hot_reload_watch", "recompile");
            (void)file_changed;
        } else if (!strcmp(file_changed, COMPILATION_RED_HERRING)){
            mlog_info("hot_reload_watch", "cancelled");
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
    printf("found_idx: %zu\n", found_idx);
    
    if (found_idx == (size_t)-1) {
        size_t old_c = t->t.capacity;
        t->t.capacity *= 2;
        t->t.array = realloc(t->t.array, t->t.chunk_size * t->t.capacity);
        t->occupied = realloc(t->occupied, t->t.capacity);
        memset(t->occupied + old_c, 0, old_c);
       
       
        found_idx = old_c;
    }

    printf("found_idx: %zu\n", found_idx);

    assert(found_idx != (size_t)-1);
    memcpy((app_context*)t->t.array + found_idx, ctx, sizeof(app_context));
    t->occupied[found_idx] = 1;
    t->t.size++;
}


static void _free_contexes_table() {
    for (size_t i = 0; i < APP_CTX_TABLE.t.capacity; i++)
    {
        if (APP_CTX_TABLE.occupied[i] != 1) continue;
        app_context* rte = (app_context*)APP_CTX_TABLE.t.array + i;
        router_destroy(&rte->router);
        dlclose(rte->dl_handle);
    }
    
    free(APP_CTX_TABLE.t.array);
}

static void _register_context(router* r, void* dl_handle) {

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
    app_context c = {
        .router = *r,
        .dl_handle = dl_handle,
        .reload_version = APP_CURR_VERSION 
    };
    free(r);
    _append_context(&g_app_context_table, &c);
}

int load_app(app_config* config) {
    void* app_handle = dlopen(APP_SO, RTLD_LAZY | RTLD_LOCAL);
    if (!app_handle) {
        mlog_error("load_app","Couldn't load application: %s", dlerror());
        return 0;
    }
    app_entry_fn entry_point = dlsym(app_handle, config->app.entry_point);
    if (!entry_point) {
        mlog_error("load_app","Couldn't find application entry point. %s", dlerror());
        return 0;
    }
    // Call app entry.
    router* r = entry_point();
    if (!r) {
        mlog_error("load_app", "null pointer received from entry point");
        return 0;
    }

    if (r->signature != ROUTER_SIGNATURE) {
        mlog_error("load_app", "pointer received from entry point (%p) was not a router ", r);
        return 0;
    }
    // Increase version
    APP_CURR_VERSION += 1;


    _register_context(r, app_handle);
    return 1;
}

app_context* load_get_ctx_with_version(uint32_t ver) {
    mlog_info("ll", "hey");
    for (size_t i = 0; i < APP_CTX_TABLE.t.capacity; i++)
    {
        if (APP_CTX_TABLE.occupied[i] == 0) continue;
        app_context* rte = &((app_context*)APP_CTX_TABLE.t.array)[i];
        if (rte->reload_version == ver) {
            return rte;
        }
    }
    return NULL;
}