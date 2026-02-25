#include <dlfcn.h>
#include <string.h>
#include <errno.h>
#include <sys/inotify.h>

#include <asclib/core.h>
#include <asclib/net/tcp.h>
#include <asclib/sync.h>
#include <asclib/cleanup.h>

#include <crefl/core.h>
#include <crefl/serial/formats.h>

#include "private/log.h"
#include "public/types.h"
#include "public/router.h"
#include "private/types.h"
#include "private/router.h"


oneshot_tx_ref_t g_shutdown_tx;

oneshot_tx_ref_t server_get_graceful_shutdown_tx() { return g_shutdown_tx; }


static task_handle_t spawn_task_on_fptr(async_fn fptr, void* arg);

void send_response_back(int fd, int code, u8_vec* buffer, u8_vec* body) {

    if (body->data) {
        snprintf((char*)buffer->data, buffer->capacity, "%d %s %.*s", code, 
            refl_enum_get_label(refl_fetch_enum("http_status_code"), code), U8_VEC_FORMAT(*body));
    } else {
        snprintf((char*)buffer->data, buffer->capacity, "%d %s", code, 
            refl_enum_get_label(refl_fetch_enum("http_status_code"), code));
    }

    buffer->count = strlen((const char*)buffer->data); 

    ssize_t sz = io_timeout(ssize_t, asc_send(fd, buffer->data, buffer->count), 200, {
        mlog_warn("client_handler", "<%d> timed out", fd);
        return;
    });
    if (sz < 0) {
        mlog_warn("client_handler", "couldn't send to client: %s", strerror(errno));
    }
}

static inline void trim_buffer(u8_vec* str) {
    char* _ptr = (char*)str->data;
    while(_ptr < (char*)str->data + str->count) {
        switch (*_ptr)
        {
        case ' ':
        case '\n':
        case '\0':
        case '\t':
        case '\r':
            *_ptr = '\0';
            break;
        
        default:
            _ptr++;
            continue;
        }
        break;
    }
    str->count = (size_t)(_ptr - (char*)str->data);
}

/**
 * @brief mock route protocol <METHOD> <ROUTE> -> GET /about
 * TEMP client_handler
 */
async(client_handler, int fd) {
    detach;
    int fd = clean_on_drop(arg(fd));
    size_t version = reload_version;
    char __b[1024];
    u8_vec buff = U8_VEC_NEW(__b, 1024);

    ssize_t sz = io_timeout(ssize_t, asc_recv(fd, buff.data, buff.capacity), 10000, {
        mlog_warn("client_handler", "<%d> timed out", fd);
        task_return_void();
    });

    buff.count = sz;

    if (sz >= 0) {
        //mlog_info("client_handler", "Received from <%d>: %.*s", fd, U8_VEC_FORMAT(buff));
    } else {
        mlog_error("client_handler", "Couldn't receive from client: %s", strerror(errno));
        task_return_void();
    }

    if (sz == 0) {
        send_response_back(fd, HTTP_BAD_REQUEST, &buff, &U8_VEC_NULL);
    }

    uint8_t* space = (uint8_t*)strchr((char*)buff.data, ' ');
    if (!space) {
        send_response_back(fd, HTTP_BAD_REQUEST, &buff, &U8_VEC_NULL);
        task_return_void();
    }
    size_t method_sz = (size_t)(space - buff.data);
    u8_vec method = U8_VEC_SUBSTR(buff.data, 0, method_sz);
    u8_vec route = U8_VEC_SUBSTR(buff.data, method_sz + 1, buff.count);

    trim_buffer(&route);


    routing_result r = mcfw_get_handler(&method, &route, version);

    if (r.st == MCFW_OK) {
        http_response* res =  await_ret(http_response*, spawn_task_on_fptr(r.data.method_fptr, NULL));
        if (!res) { 
            send_response_back(fd, HTTP_INTERNAL_SERVER_ERROR, &buff, &U8_VEC_NULL);
            mlog_error("client_handler", "null ptr received from method: %.*s", U8_VEC_FORMAT(method)); 
            task_return_void(); 
        } else if (res->signature != HTTP_RES_SIGNATURE) {
            mlog_error("client_handler", "http response received from method: %.*s is garbage", U8_VEC_FORMAT(method));
        }
        send_response_back(fd, res->code, &buff, &res->buff);
        free(res->buff.data);
        free(res);
        task_return_void();
    }

    send_response_back(fd, r.data.err, &buff, &U8_VEC_NULL);

    end_return_void();
}

async(accept_loop, tcp_listener_t* t) {
    tcp_listener_t* t = arg(t);
    int shutdown_payload;
    oneshot_pair_t graceful_shutdown = oneshot_new(sizeof(int), 0);
    mlog_info("accept_loop", "Listening on %hhu.%hhu.%hhu.%hhu:%hu", 
        t->ip_addr[0],
        t->ip_addr[1],
        t->ip_addr[2],
        t->ip_addr[3],
        t->port
    );
    g_shutdown_tx = oneshot_move_tx(graceful_shutdown);
    while(1) {
        int client_fd = any_expect_winner_or(
            int, 1,
            await_any(
                asc_oneshot_recv((oneshot_rx_t)graceful_shutdown, &shutdown_payload),
                asc_tcp_accept(t)).cut_losers(), 
            {
                mlog_warn("accept_loop", "Triggered server graceful shutdown, payload: 0x%X", shutdown_payload);
                break;
            }
        );
        if (client_fd >= 0) {
            task_spawn(client_handler, .fd = client_fd);
        }
    }
    oneshot_drop_rx((oneshot_rx_t)graceful_shutdown);
    end_return_void();
}

static task_handle_t spawn_task_on_fptr(async_fn fptr, void* arg) {
    task__arg__constructor te =  {
        .arg_malloced = 1,
        .arg_size = sizeof(void*),
        .fptr = *fptr,
        .n_pages = TASK_DEFAULT_PAGES,
        .value = arg
    };
    return __task_create(te);
}

/**
 * @todo TEMP!! change to hot-reloading thing
 */
void* temp_g_handle = NULL;
static void _temp_close_app() { if(temp_g_handle)dlclose(temp_g_handle); }

static int load_app(app_config* config) {
    void* app_handle = dlopen(APP_SO, RTLD_LAZY | RTLD_LOCAL);
    if (!app_handle) {
        mlog_error("load_app","Couldn't load application: %s", dlerror());
        return 0;
    }
    async_fn entry_point = dlsym(app_handle, config->app.entry_point);
    if (!entry_point) {
        mlog_error("load_app","Couldn't find application entry point. %s", dlerror());
        return 0;
    }
    router* r = await_ret(router*, spawn_task_on_fptr(entry_point, NULL));
    mcfw_register_router(r);
    //TEMP!!
    temp_g_handle = app_handle;
    atexit(_temp_close_app);
    return 1;
}

decl_async(void, hot_reload_watch);

async(server_set_tcp, app_config* config) {
    app_config* config = arg(config);
    uint8_t addr[4];
    uint16_t port = config->host.port;
    memcpy(addr, &config->host.addr, 4);

    // Let the reflection engine work on this thread
    refl_threadlocal_init(0);

    tcp_listener_t* t = clean_on_drop(asc_tcp_bind(addr, port));
    if (!t) {
        mlog_panic("server_set_tcp", "couldn't create tcp listener: %s", strerror(errno));
        task_return(int, 1);
    }


    load_app(config);
    task_handle_t t_watcher;
    if (g_args.hr_enabled) t_watcher = task_spawn(hot_reload_watch);
    await(task_spawn(accept_loop, .t = t));

    if (g_args.hr_enabled) drop_task(t_watcher);
    end_return(int, 0);
}


void server_start() {


    worker_t* w = worker_create(1, task_entry(server_set_tcp, .config = g_app_config));
    if (g_args.no_ss) {
        worker_disable_sigsegv(w);
    }
    worker_run(w);
    int exit_code = task_recover_from_worker(worker_await_all(w), 0, int);
    if (exit_code != 0) {
        _Exit(exit_code);
    } 
    mlog_info("server_start", "Server closed.");
    worker_destroy(w);
}