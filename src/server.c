#include <dlfcn.h>
#include <string.h>
#include <errno.h>

#include <asclib/core.h>
#include <asclib/net/tcp.h>
#include <asclib/sync.h>
#include <asclib/cleanup.h>


#include "configs.h"
#include "log.h"
#include "public/types.h"
#include "public/router.h"


async(client_handler, int fd) {
    detach;
    int fd = clean_on_drop(arg(fd));
    char buff[1024];

    ssize_t sz = io_timeout(ssize_t, asc_recv(fd, buff, 1024), 10000, {
        mlog_warn("client_handler", "<%d> timed out", fd);
        task_return_void();
    });

    if (sz >= 0) {
        mlog_info("client_handler", "Received from <%d>: %.*s", fd, (int)sz, buff);
    } else {
        mlog_error("client_handler", "Couldn't receive from client: %s", strerror(errno));
        task_return_void();
    }

    sz = io_timeout(ssize_t, asc_send(fd, "Pong!\n", 7), 200, {
        mlog_warn("client_handler", "<%d> timed out", fd);
        task_return_void();
    });
    if (sz < 0) {
        mlog_error("client_handler", "couldn't send to client: %s", strerror(errno));
    }

    end_return_void();
}

async(accept_loop, tcp_listener_t* t) {
    int c = 0;
    tcp_listener_t* t = arg(t);
    mlog_info("accept_loop", "Listening on %hhu.%hhu.%hhu.%hhu:%hu", 
        t->ip_addr[0],
        t->ip_addr[1],
        t->ip_addr[2],
        t->ip_addr[3],
        t->port
    );
    while(c < 2) {
        int client_fd = await_ret(int, asc_tcp_accept(t));
        if (client_fd >= 0) {
            task_spawn(client_handler, .fd = client_fd);
        }
        c++;
    }
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
    dlclose(app_handle);
    return 1;
}


async(server_set_tcp, app_config* config) {
    app_config* config = arg(config);
    uint8_t addr[4];
    uint16_t port = config->host.port;
    memcpy(addr, &config->host.addr, 4);
    tcp_listener_t* t = clean_on_drop(asc_tcp_bind(addr, port));
    if (!t) {
        mlog_panic("server_set_tcp", "couldn't create tcp listener: %s", strerror(errno));
        task_return(int, 1);
    }


    load_app(config);

    await(task_spawn(accept_loop, .t = t));
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