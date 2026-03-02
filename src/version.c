
#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <dlfcn.h>
#include <sys/inotify.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

#include <asclib/core.h>
#include <asclib/sync.h>

#include <crefl/core.h>
#include <crefl/scope.h>

#include "private/configs.h"
#include "private/version.h"
#include "private/log.h"
#include "private/glob.h"
#include "private/router.h"

#define MAX(a, b) ((a) > (b))? (a) : (b)

#define INITIAL_CONTEXES_CAPACITY 4
#define INITIAL_SCOPES_CAPACITY 4

#define APP_RELOAD_VER_INITIAL_VAL (uint32_t)-1

app_context_table APP_CTX_TABLE = {0};
crefl_scope_table CREFL_SCOPES_TABLE = {0};
uint32_t APP_CURR_VERSION = APP_RELOAD_VER_INITIAL_VAL;

async_(version_watch_async) {
    detach;
    pid_t parent_pid = getppid();
    char* watch_folder = (char*)APP_CONFIG_STRUCT->app.source_folder;
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
            load_app(); // TEMP!! think for a better system because this triggers false negatives.
            remove(COMPILATION_RED_HERRING);
        }
        free(file_changed);
        // 50 ms sleep to help things get in their place.
        await(asc_sleep(50));
    }
    end_return_void();
}

static crefl_scope* _register_scope(void* crefl_handle, uint8_t* checksum) {
    crefl_scope s = {.c_handle = crefl_handle, .c_ctx_ref_count = 0};
    crefl_scope_table* t = &CREFL_SCOPES_TABLE;
    memcpy(&s.c_checksum, checksum, 16);
    crefl_scope* malloced = calloc(sizeof(crefl_scope), 1);
    *malloced = s;

    size_t found_idx = (size_t)-1;
    for (size_t i = 0; i < t->t.capacity; i++)
    {
        if (t->occupied[i] == 0) { found_idx = i; break;}
    }

    if (found_idx == (size_t)-1) {
        size_t old_c = t->t.capacity;
        t->t.capacity *= 2;
        mlog_info("realloc: %zu -> %zu", old_c, t->t.capacity);
        t->t.array = realloc(t->t.array, t->t.capacity * sizeof(crefl_scope*));
        memset((void**)t->t.array + old_c, 0, old_c * sizeof(void*));
        t->occupied = realloc(t->occupied, t->t.capacity);
        memset(t->occupied + old_c, 0, old_c);
        found_idx = old_c;
    }

    assert(found_idx != (size_t)-1);

    t->occupied[found_idx] = 1;
    ((crefl_scope**)t->t.array)[found_idx] = malloced;
    malloced->at_index = found_idx;
    t->t.size++;
    return ((crefl_scope**)t->t.array)[found_idx];
}


static void _drop_scope(crefl_scope* s) {
    mlog_info("+");
    size_t idx = s->at_index;

    if (CREFL_SCOPES_TABLE.occupied[idx] == 0) return;

    refl_close_scope(s->c_handle);
    free(s);
    // Mark as free.
    ((void**)CREFL_SCOPES_TABLE.t.array)[idx] = NULL;
    CREFL_SCOPES_TABLE.occupied[idx] = 0;

    mlog_info("-");
}


static void _append_context(app_context* ctx) {
    size_t found_idx = (size_t)-1;
    app_context_table* t = &APP_CTX_TABLE;
    for (size_t i = 0; i < t->t.capacity; i++)
    {
        if (t->occupied[i] == 0) {found_idx = i; break;}
    }

    if (found_idx == (size_t)-1) {
        size_t old_c = t->t.capacity;
        t->t.capacity *= 2;
        mlog_info("realloc: %zu -> %zu", old_c, t->t.capacity);
        t->t.array = realloc(t->t.array, t->t.chunk_size * t->t.capacity);
        memset((void**)t->t.array + old_c, 0, old_c * sizeof(void*));
        t->occupied = realloc(t->occupied, t->t.capacity);
        memset(t->occupied + old_c, 0, old_c);
       
       
        found_idx = old_c;
    }

    assert(found_idx != (size_t)-1);
    ((app_context**)t->t.array)[found_idx] = ctx;
    ctx->at_index = found_idx;
    t->occupied[found_idx] = 1;
    t->t.size++;
    
}

/**
 * @brief Free an app context from the table. 
 */
void version_drop_ctx(app_context*  a, int try_drop_scope) {
    mlog_info("+");

    size_t idx = a->at_index;

    a->crefl_data->c_ctx_ref_count--;


    router_destroy(&a->router);
    //dlclose(a->dl_handle);
    if (a->crefl_data->c_ctx_ref_count == 0 && try_drop_scope) {
        _drop_scope(a->crefl_data);
    }

    free(a);
    // Mark as free;
    ((void**)APP_CTX_TABLE.t.array)[idx] = NULL;
    APP_CTX_TABLE.occupied[idx] = 0;
    mlog_info("-");
}


static void _free_tables_exit() {
    for (size_t i = 0; i < APP_CTX_TABLE.t.capacity; i++)
    {
        if (APP_CTX_TABLE.occupied[i] != 1) continue;
        version_drop_ctx(((app_context**)APP_CTX_TABLE.t.array)[i], 1);
    }
    free(CREFL_SCOPES_TABLE.t.array);
    free(CREFL_SCOPES_TABLE.occupied);
    free(APP_CTX_TABLE.t.array);
    free(APP_CTX_TABLE.occupied);
}

static void _register_context(router* r, void* dl_handle, crefl_scope* c_data) {
    router rout = (r != NULL)? *r : (router){0};

    app_context c = {
        .router = rout,
        .dl_handle = dl_handle,
        .crefl_data = c_data,
        .ref_count = 0,
        .at_index = (size_t)-1,
        .reload_version = APP_CURR_VERSION 
    };
    free(r);
    c_data->c_ctx_ref_count++;
    app_context* malloced = calloc(sizeof(app_context), 1);
    *malloced = c;
    _append_context(malloced);

}

#if defined(__GNUC__) || defined(__clang__)
#  define CREFL_PACKED __attribute__((packed))
#elif defined(_MSC_VER)
#  define CREFL_PACKED __pragma(pack(push, 1))
#else
#  error "Unknown compiler"
#endif

typedef struct CREFL_PACKED
{
    uint64_t    magic;             
    uint8_t     endianness;        
    uint16_t    version;           
    uint16_t    flags;             
    uint8_t     checksum[16];
    uint32_t    n_sections;        
} CREFL_Header;

static int _crefl_needs_reloading(app_context* curr_ver, uint8_t* out_checksum) {

    int fd = open(CREFL_APP_FILE, O_RDONLY);
    if (fd < 0) mlog_error("Couldn't find reflected type metadata file ("CREFL_APP_FILE").");
    
    CREFL_Header h;
    ssize_t n = read(fd, &h, sizeof(CREFL_Header));
    if (n < 0) mlog_error("Couldn't read from .crefl file: %s", strerror(errno));
    close(fd);

    // Compare the checksums
    memcpy(out_checksum, h.checksum, 16);
    if (curr_ver != NULL) {
        int needs = memcmp(curr_ver->crefl_data->c_checksum, h.checksum, 16) != 0;
        return needs;
    }
    return 1;
}

static inline void init_tables() {
    // Allocate contexes table if is not set yet
    if (!APP_CTX_TABLE.t.array) {
        APP_CTX_TABLE.t = (r_array_t){
            .array = calloc(sizeof(app_context*), INITIAL_CONTEXES_CAPACITY),
            .capacity = INITIAL_CONTEXES_CAPACITY,
            .size = 0,
            .chunk_size = sizeof(app_context*)
        };
        APP_CTX_TABLE.occupied = malloc(INITIAL_CONTEXES_CAPACITY);
        memset(APP_CTX_TABLE.occupied, 0, INITIAL_CONTEXES_CAPACITY);
        atexit(_free_tables_exit);
    }

    // Allocate crefl metadata table aswell
    if (!CREFL_SCOPES_TABLE.t.array) {
        CREFL_SCOPES_TABLE.t = (DYN_ARRAY(crefl_scope*)){
            .array = calloc(sizeof(crefl_scope*), INITIAL_SCOPES_CAPACITY),
            .capacity = INITIAL_SCOPES_CAPACITY,
            .size = 0,
            .chunk_size = sizeof(crefl_scope*)
        };
        CREFL_SCOPES_TABLE.occupied = malloc(INITIAL_SCOPES_CAPACITY);
        memset(CREFL_SCOPES_TABLE.occupied, 0, INITIAL_SCOPES_CAPACITY);
    }

}


int load_app() {
    uint32_t next_version = APP_CURR_VERSION + 1;

    // Will be NULL in the first call to this function.
    app_context* curr_ver = version_get_ctx(APP_CURR_VERSION);

    // Allocate tables if they hadn't been allocated yet.
    init_tables();

    // Generate a unique name for the current .so and hard link with the original. 
    // dlopen() will not load a new one without this trick.
    char unique_name[64];
    snprintf(unique_name, 64, APP_LIBS_PREFIX"_v%u.so", next_version);
    int n = link(APP_SO, unique_name);
    (void)n;

    void* app_handle = dlopen(unique_name, RTLD_NOW | RTLD_LOCAL);
    if (!app_handle) {
        mlog_error("Couldn't load application: %s", dlerror());
        unlink(unique_name);
        goto __exit_dont_apply;

    }
    // No need to keep it around.
    unlink(unique_name);

    app_entry_fn entry_point = dlsym(app_handle, APP_CONFIG_STRUCT->app.entry_point);
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


    // Advance version.
    APP_CURR_VERSION = next_version;
    
    // If types data hasn't changed we use the old handle.
    uint8_t checksum[16];
    RHandle h = NULL;
    int needs = _crefl_needs_reloading(curr_ver, checksum); (void)needs;
    
    crefl_scope* old_sc = NULL;
    if (curr_ver) old_sc = curr_ver->crefl_data;

    // Drop old version
    if (APP_CURR_VERSION - 1 != APP_RELOAD_VER_INITIAL_VAL) {
        // If there are no more clients, free the old context
        assert(curr_ver);
        // Try to drop the crefl scope if we arent going to need it anymore
        if (curr_ver->ref_count == 0)mlog_info("dropped from here");
        if (curr_ver->ref_count == 0) version_drop_ctx(curr_ver, 1);
    }


    if (!1) {
        assert(old_sc);
        _register_context(r, app_handle, old_sc);
        return 1;
    }

    h = refl_load_metadata_to(CREFL_APP_FILE);
    assert(h);
    _register_context(r, app_handle, _register_scope(h, checksum));
    if (old_sc && old_sc->c_ctx_ref_count == 0) {
        assert(CREFL_SCOPES_TABLE.occupied[old_sc->at_index] != 0);
        _drop_scope(old_sc);
    }
    return 1;


__exit_dont_apply:
    if (app_handle) dlclose(app_handle);
    return 0;
}

app_context* version_get_ctx(uint32_t ver) {
    app_context* rte = NULL;
    for (size_t i = 0; i < APP_CTX_TABLE.t.capacity; i++)
    {
        if (APP_CTX_TABLE.occupied[i] == 0) continue;
        rte = ((app_context**)APP_CTX_TABLE.t.array)[i];
        if (rte->reload_version == ver) {
            return rte;
        }
    }
    return NULL;
}