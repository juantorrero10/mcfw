#include <string.h>
#include <errno.h>

#include <asclib/core.h>
#include <asclib/net/tcp.h>
#include <asclib/sync.h>
#include <asclib/cleanup.h>

#include <crefl/core.h>
#include <crefl/scope.h>
#include <crefl/serial/formats.h>

#include "private/log.h"
#include "public/types.h"
#include "public/router.h"
#include "public/http_interface.h"
#include "private/router.h"
#include "private/version.h"


oneshot_tx_ref_t g_shutdown_tx;

oneshot_tx_ref_t server_get_graceful_shutdown_tx() { return g_shutdown_tx; }

static task_handle_t spawn_task_on_fptr(async_fn fptr, void* arg);

struct method_args {
    size_t padding;
    http_request req;
};

#define STRIP_HTTP_PREFIX(_str_expr) (_str_expr + 5)

void send_response_back(int fd, int code, u8_vec* buffer, u8_vec* body) {

    if (body->data) {
        snprintf((char*)buffer->data, buffer->capacity, "HTTP/1.0 %d %s\r\n%.*s\r\n", 
            code, 
            STRIP_HTTP_PREFIX(
                refl_enum_get_label(
                    refl_fetch_enum("http_status_code"), 
                    code
                )
            ), 
            U8_VEC_FMT(*body));
    } else {
        snprintf((char*)buffer->data, buffer->capacity, "HTTP/1.0 %d %s\r\n", code, 
            STRIP_HTTP_PREFIX(refl_enum_get_label(refl_fetch_enum("http_status_code"), code)));
    }

    buffer->count = strlen((const char*)buffer->data); 

    ssize_t sz = io_timeout(ssize_t, asc_send(fd, buffer->data, buffer->count), 200, {
        mlog_warn("<%d> timed out", fd);
        return;
    });
    if (sz < 0) {
        mlog_warn("couldn't send to client: %s", strerror(errno));
    }
}

static inline void trim_slice(u8_slice* str) {
    char* _ptr = (char*)str->data;
    while(_ptr < (char*)str->data + str->len) {
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
    str->len = (size_t)(_ptr - (char*)str->data);
}

/**
 * @brief mock route protocol <METHOD> <ROUTE> [<body>] -> GET /about, POST /getuser {"id":2}
 * TEMP client_handler
 */
async(client_handler, int fd) {
    detach;
    int fd = clean_on_drop(arg(fd));

    // Buffer to get the user response.
    // This buffer will be reuse when sending the response back if posible.
    char __b[1024];
    u8_vec buff = {.data = (uint8_t*)__b, .capacity = 1024, .count = 0};
    
    // Hot reloading: get the current version and get the most recent router.
    size_t version = APP_CURR_VERSION;
    app_context* c = version_get_ctx(version);
    if (!c) { 
        send_response_back(fd, HTTP_INTERNAL_SERVER_ERROR, &buff, &U8_VEC_NULL);
        task_return_void();
    }
    // Increase reference count the app ctx.
    c->ref_count++;
    router* rout = &c->router;
    

    ssize_t sz = io_timeout(ssize_t, asc_recv(fd, buff.data, buff.capacity), 60000, {
        mlog_warn("<%d> timed out", fd);
        task_return_void();
    });

    if (sz < 0) {
        mlog_error("Couldn't receive from client: %s", strerror(errno));
        goto __dec_rc_and_exit;
    } else if (sz == 0) {
        send_response_back(fd, HTTP_BAD_REQUEST, &buff, &U8_VEC_NULL);
    }

    buff.count = sz;

    uint8_t* space = (uint8_t*)strchr((char*)buff.data, ' ');
    if (!space) {
        send_response_back(fd, HTTP_BAD_REQUEST, &buff, &U8_VEC_NULL);
        goto __dec_rc_and_exit;
    }
    size_t method_sz = (size_t)(space - buff.data);
    size_t route_sz = 0;
    u8_slice method = U8_VIEW(buff.data, 0, method_sz);
    space = (uint8_t*)strchr((char*)buff.data + method_sz + 1, ' ');
    int has_body = space != NULL;
    route_sz = (has_body)? (size_t)(space - buff.data) : buff.count;
    u8_slice route = U8_VIEW(buff.data, method_sz + 1, route_sz);
    u8_slice body = (u8_slice){0};
    if (has_body) {
        body = U8_VIEW(buff.data, route_sz + 1, buff.count);
    }

    trim_slice(&route);


    routing_result r = router_get_handler(rout, method, route);

    if (r.st == MCFW_OK) {
        struct method_args a = {0, http_req_create(CONTENT_TEXT, body)};
        a.req.crefl_handle = c->crefl_data->c_handle;
        http_response* res =  await_ret(http_response*, spawn_task_on_fptr(r.data.method_fptr, &a));
        if (!res) { 
            send_response_back(fd, HTTP_INTERNAL_SERVER_ERROR, &buff, &U8_VEC_NULL);
            mlog_error("null ptr received from method: %.*s", U8_SLICE_FMT(method)); 
            goto __dec_rc_and_exit;
        } else if (res->signature != HTTP_RES_SIGNATURE) {
            mlog_error("http response received from method: %.*s is garbage", U8_SLICE_FMT(method));
        }
        send_response_back(fd, res->code, &buff, &res->buff);
        free(res->buff.data);
        free(res);
        goto __dec_rc_and_exit;
    }

    send_response_back(fd, r.data.err, &buff, &U8_VEC_NULL);

__dec_rc_and_exit:
    c->ref_count--;
    if (c->ref_count == 0 && APP_CURR_VERSION != version)
        version_drop_ctx(c, 1);
    end_return_void();
}

async(accept_loop, tcp_listener_t* t) {
    tcp_listener_t* t = arg(t);
    int shutdown_payload;
    oneshot_pair_t graceful_shutdown = oneshot_new(sizeof(int), 1);
    mlog_info("Listening on %hhu.%hhu.%hhu.%hhu:%hu", 
        t->ip_addr[0],
        t->ip_addr[1],
        t->ip_addr[2],
        t->ip_addr[3],
        t->port
    );
    g_shutdown_tx = oneshot_move_tx(graceful_shutdown);
    while(1) {
        int client_fd = any_expect_winner_or(
            int, 0,
            await_any(
                asc_tcp_accept(t),
                asc_oneshot_recv((oneshot_rx_t)graceful_shutdown, &shutdown_payload)).cut_losers(),
            {
                mlog_warn("Triggered server graceful shutdown, payload: 0x%X", shutdown_payload);
                break;
            }
        );
        if (client_fd >= 0) {
            task_spawn(client_handler, .fd = client_fd);
        }
    }
    oneshot_drop_rx((oneshot_rx_t)graceful_shutdown);
    oneshot_assert_closed(graceful_shutdown);
    end_return_void();
}

static task_handle_t spawn_task_on_fptr(async_fn fptr, void* arg) {


    task__arg__constructor te =  {
        .arg_malloced = 0,
        .arg_size = sizeof(void*),
        .fptr = *fptr,
        .n_pages = TASK_DEFAULT_PAGES,
        .value = arg
    };
    return __task_create(te);
}

async(server_set_tcp) {
    uint8_t addr[4];
    uint16_t port = APP_CONFIG_STRUCT->host.port;
    memcpy(addr, &APP_CONFIG_STRUCT->host.addr, 4);

    // Let the reflection engine work on this thread
    refl_threadlocal_init(0);

    tcp_listener_t* t = clean_on_drop(asc_tcp_bind(addr, port));
    if (!t) {
        mlog_panic("couldn't create tcp listener: %s", strerror(errno));
        task_return(int, 1);
    }


    load_app();
    task_handle_t t_watcher;
    if (ARG_FLAGS.hr_enabled) t_watcher = task_spawn(version_watch_async);
    await(task_spawn(accept_loop, .t = t));

    if (ARG_FLAGS.hr_enabled) drop_task(t_watcher);
    end_return(int, 0);
}


void server_start() {


    worker_t* w = worker_create(1, task_entry(server_set_tcp));
    if (ARG_FLAGS.no_ss) {
        worker_disable_sigsegv(w);
    }
    worker_run(w);
    int exit_code = task_recover_from_worker(worker_await_all(w), 0, int);
    if (exit_code != 0) {
        _Exit(exit_code);
    } 
    mlog_info("Server closed.");
    worker_destroy(w);
}