#include <private/mcfw.h>
#include <asclib/core.h>
#include <asclib/env.h>

#define MIN(a, b) ((a) < (b))? (a) : (b)

// Static
static bool s_server_has_shutdown = false;

// TODO: if asclic changes to multithreading, put a mutex
static uint64_t s_n_clients = 0;
_Public_ uint64_t server_get_total_clients() { return s_n_clients; }

// TODO: if asclic changes to multithreading, put a mutex
static oneshot_tx_ref_t s_shutdown_oneshot = NULL;
_Public_ void server_stop() {
    oneshot_tx_t tx = oneshot_take_tx(s_shutdown_oneshot);
    uint64_t payload = 0xDEADBEEF;
    (void)await_ret(chan_err_t, asc_oneshot_send(tx, &payload));
    oneshot_drop_tx(tx);
    s_server_has_shutdown = true;
}

// Task local
ENV_DECLARE(AppContext*, ctx);
AppContext* server_get_ctx() { return ENVGET_ctx();}
void* server_get_crefl_handle() { return ENVGET_ctx()->crefl_data->c_handle; }


async(client_handler, int fd) {
    detach;
    int fd = clean_on_drop(arg(fd));

    s_n_clients++;
    ENVSET_ctx(NULL);
    
    // Init arena
    Arena arena = mcfw_arena_init(RECV_BUFF_SIZE);
    

    // main objects
    HttpStatusCode status = HTTP_OK;
    HttpRequest req = {0};
    HttpResponse res = res_init(fd);
    
    // Get app context
    size_t version = APP_CURR_VERSION;
    AppContext* ctx = version_get_ctx(APP_CURR_VERSION);
    if (!ctx) {
        res.code = HTTP_INTERNAL_SERVER_ERROR;
        res_send(&res, SEND_TIMEOUT);
        goto __exit_err;
    }
    ctx->ref_count++;
    ENVSET_ctx(ctx);

    // buffers init
    DStatic headers_buff;
    DStatic unused_data; // Part of the body or next request if pipelined

    int64_t n_requests = 0;
    
    while(1) {
        
        // Dont process anymore requests
        if (s_server_has_shutdown) {
            break;
        }
        bool eof = false;
        status = req_recv_headers(
            &arena,
            fd,
            &req,
            &headers_buff, 
            &unused_data,
            &eof
        );
        if (status != HTTP_OK) {
            res.code = status;
            res.keep_alive = false;
            if (!eof) {
                res_send(&res, SEND_TIMEOUT);
            }
            goto __exit_err;
        }


        // Get route handle
        Router* router = &ctx->router;
        RouterEntry endpoint = (RouterEntry){0};
        status = router_get_entry(&arena, router, &req, &endpoint);
        if (status != HTTP_OK) {
            res.code = status;
            // Keep connections alive for 4xxs from GET, OPTIONS, HEAD and DELETE
            if (req.r_method < PAYLOAD_METHODS_IDX && 
                status >= HTTP_REQ_ERRORS_START &&
                status < HTTP_SERVER_ERRORS_START
            ) {
                goto __loop;
            }
            res.keep_alive = false;
            res_send(&res, SEND_TIMEOUT);
            goto __exit_err;
        }

        if (req.r_method == HEAD && endpoint.r_method == GET) {
            res.send_body = false;
        }

        status = req_fill_core_headers(&req, endpoint.expect);
        if (status != HTTP_OK) {
            res.code = status;
            res.keep_alive = false;
            res_send(&res, SEND_TIMEOUT);
            goto __exit_err;
        }

        // Get body
        if (req.r_method >= PAYLOAD_METHODS_IDX) {
            __recv_body:
            status = req_recv_body(
                fd, 
                &req, 
                ctx, 
                endpoint.deserialize_typename, 
                &unused_data,
                endpoint.deserialize
            );
            if (status == HTTP_CONTINUE) {
                goto __recv_body;
            }
            if (status != HTTP_OK) {
                res.code = status;
                res.keep_alive = false;
                res_send(&res, SEND_TIMEOUT);
                goto __exit_err;
            }
            // When receiving the body we know how much to read
            // So there will be no unused data
            unused_data = DS_EMPTY();
        }

        // Set some headers from the request because the response needs them
        res_set_req_headers(&res, 
            req.core_headers.conn_keep_alive,
            req_get_header_value(&req, DS_LIT("Range"))
        );

        // Allocate the space for the app's custom headers
        res_allocate_headers(&res, &arena);

        
        // Execute handler
        switch (endpoint.handler.kind)
        {
        case ROUTER_FUNC:
            if (endpoint.handler.method_fptr == NULL) {
                mlog_error("The handler responsible for route \"%.*s\" is a NULL pointer.", DS_ARGS(endpoint.route_str));
                res.code = HTTP_INTERNAL_SERVER_ERROR;
                res.keep_alive = false;
                res_send(&res, SEND_TIMEOUT);
                goto __exit_err;
            }
            endpoint.handler.method_fptr(&req, &res);
            break;
        case ROUTER_FILE:
            if (endpoint.handler.path.s_ptr == NULL) {
                mlog_error("The file returned by route \"%.*s\" is a NULL pointer.", DS_ARGS(endpoint.route_str));
                res.code = HTTP_INTERNAL_SERVER_ERROR;
                res.keep_alive = false;
                res_send(&res, SEND_TIMEOUT);
                goto __exit_err;
            }
            static_file_handler(&req, &res, &endpoint.handler.path, endpoint.expect);
            break;
        
        default:
            break;
        }

        // Send response, cleanup and loop
__loop:
        n_requests++;
        if (n_requests >= MAX_REQ_PER_CONNECTION || s_server_has_shutdown) {
            res.keep_alive = false;
        }
        res_send(&res, SEND_TIMEOUT);

        mcfw_arena_reset(&arena);
        if (!res.keep_alive) break;
        // Move data that we have read accidentally to the start of the buffer.
        if (DS_IS_VALID(unused_data)) {
            mcfw_arena_alloc(&arena, unused_data.count, true);
            memmove(arena.p, unused_data.s_ptr, unused_data.count);
        }
        req_destroy(&req);
        res_destroy(&res);
        // loop
    }
    
// Final cleanup
__exit_err:
    ctx->ref_count--;
    if (ctx->ref_count == 0 && APP_CURR_VERSION != version) {
        version_drop_ctx(ctx, 1);
    }
    s_n_clients--;
    shutdown(fd, SHUT_RDWR);
    mcfw_arena_destroy(&arena);
    req_destroy(&req);
    res_destroy(&res);
    end_return_void();
}



async(accept_loop, tcp_listener_t* t) {
    tcp_listener_t* t = arg(t);
    uint64_t shutdown_payload;
    oneshot_pair_t graceful_shutdown = oneshot_new(sizeof(uint64_t), true);
    mlog_info("Listening on %hhu.%hhu.%hhu.%hhu:%hu", 
        t->ip_addr[0],
        t->ip_addr[1],
        t->ip_addr[2],
        t->ip_addr[3],
        t->port
    );
    s_shutdown_oneshot = oneshot_move_tx(graceful_shutdown);
    while(1) {
        int client_fd = any_expect_winner_or(
            int, 0,
            await_any(
                asc_tcp_accept(t),
                asc_oneshot_recv((oneshot_rx_t)graceful_shutdown, &shutdown_payload)).cut_losers(),
            {
                mlog_warn("Triggered server graceful shutdown, payload: 0x%lX", shutdown_payload);
                break;
            }
        );
        if (client_fd >= 0) {
            task_spawn_ex(client_handler, 10, .fd = client_fd);
        }
    }
    oneshot_drop_rx((oneshot_rx_t)graceful_shutdown);
    oneshot_assert_closed(graceful_shutdown);
    close(t->fd);
    end_return_void();
}


async(server_init) {
    uint8_t addr[4];
    uint16_t port = APP_CONFIG_STRUCT->host.port;
    memcpy(addr, &APP_CONFIG_STRUCT->host.addr, 4);

    // Let the reflection engine work on this thread
    refl_threadlocal_init(1);

    // Load the cached reflected types
    CACHED_ENUM_REQ_KIND = refl_fetch_enum("HttpRequestMethod");
    CACHED_ENUM_STR_STATUS = refl_fetch_enum("HttpStatusCode");


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

    refl_threadlocal_destroy();

    end_return(int, 0);
}


void server_start() {


    worker_t* w = worker_create(1, task_entry(server_init));
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
