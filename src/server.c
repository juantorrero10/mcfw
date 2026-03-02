#include <string.h>
#include <errno.h>

#include <asclib/core.h>
#include <asclib/net/tcp.h>
#include <asclib/sync.h>
#include <asclib/cleanup.h>
#include <time.h>

#include <crefl/core.h>
#include <crefl/scope.h>
#include <crefl/serial/formats.h>

#include "private/log.h"
#include "public/types.h"
#include "public/router.h"
#include "public/http_interface.h"
#include "private/http.h"
#include "private/router.h"
#include "private/version.h"
#include "private/glob.h"


oneshot_tx_ref_t g_shutdown_tx;

oneshot_tx_ref_t server_get_graceful_shutdown_tx() { return g_shutdown_tx; }


static task_handle_t spawn_task_on_fptr(async_fn fptr, void* arg);

struct method_args {
    size_t padding;
    http_request req;
};

static inline char* get_gmt_date(char* write_at, size_t len) {
    time_t now = time(NULL);
    struct tm *gmt = gmtime(&now);
    // %a = Abbreviated weekday (Sun)
    // %d = Day of month (01)
    // %b = Abbreviated month (Mar)
    // %Y = Year (2026)
    // %H:%M:%S = Time
    strftime(write_at, len, "%a, %d %b %Y %H:%M:%S GMT", gmt);
    return write_at;
}

#define SEND_TIMEOUT 200
#define STRIP_HTTP_PREFIX(_str_expr) (_str_expr + 5)

/**
 * @brief send a bodyless error back to the client.
 * This is when 404's, 405's and simple 400's are sent.
 */
void send_response_back_host_error(int fd, int code, u8_vec* buffer) {
    const char* str_status = refl_enum_get_label((REnum*)CACHED_ENUM_STR_STATUS, code);
    if (!str_status) str_status = "INTERNAL_SERVER_ERROR";

    char* fmt_template = 
        "HTTP/1.0 %03d %s\r\n"
        "Server: "SERVER_HEADER_VAL"\r\n"
        "Connection: close\r\n"
        "\r\n"
    ;
    buffer->count = snprintf((char*)buffer->data, buffer->capacity, fmt_template, code, 
            STRIP_HTTP_PREFIX(str_status));

    ssize_t sz = io_timeout(ssize_t, asc_send(fd, buffer->data, buffer->count), SEND_TIMEOUT, {
        mlog_warn("<%d> timed out", fd);
        return;
    });
    if (sz < 0) {
        mlog_warn("couldn't send to client: %s", strerror(errno));
    }
}

void send_response_back(int fd, u8_vec* buffer, http_response* res) {
    // Get content type header
    char* ctype_str = str_response_type_lut[res->type];
    if (ctype_str == NULL) ctype_str = "application/octet-stream";

    // Get string version of the error code.
    const char* str_status = refl_enum_get_label((REnum*)CACHED_ENUM_STR_STATUS, res->code);
    if (!str_status) str_status = "OK"; // Fallback case.

    char date_buff[32];

    char* fmt_template = 
        "HTTP/1.0 %03d %s\r\n"
        "Server: "SERVER_HEADER_VAL"\r\n"
        "Date: %s\r\n"
        "Connection: close\r\n"
        "Content-Type: %s\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Content-Length: %zu\r\n"
        "\r\n"
        "%.*s"
        ;
    buffer->count = snprintf((char*)buffer->data, buffer->capacity, fmt_template, 
        res->code, STRIP_HTTP_PREFIX(str_status), get_gmt_date(date_buff, 32), ctype_str, res->buff.count, U8_VEC_FMT(res->buff));

    ssize_t sz = io_timeout(ssize_t, asc_send(fd, buffer->data, buffer->count), SEND_TIMEOUT, {
        mlog_warn("<%d> timed out", fd);
        return;
    });
    if (sz < 0) {
        mlog_warn("couldn't send to client: %s", strerror(errno));
    }
}

http_status_code try_serialize_response(const app_context* c, http_response* res, u8_vec* out_buff, RStruct** out_rs) {
    RStruct* s = refl_fetch_struct_from(c->crefl_data->c_handle, res->typename);
    if (!s) {
        // The app has provided an invalid name
        mlog_error("Couldn't find reflected type: %s for serialization.", res->typename);
        return HTTP_INTERNAL_SERVER_ERROR;
    }
    if (!res->buff.data) {
        mlog_error("Tried to serialize a null pointer.");
        return HTTP_INTERNAL_SERVER_ERROR;
    }
    switch (res->type)
    {
    case CONTENT_JSON:
        out_buff->data = serialize_json(s, res->buff.data, 1);
        /**
         * @todo add out_size to crefl
         */
        out_buff->count = out_buff->capacity = strlen((char*)out_buff->data);
        if (out_rs) *out_rs = s;
        return HTTP_OK;
    
    default:
        return HTTP_INTERNAL_SERVER_ERROR;
        break;
    }
    
}

http_status_code try_deserialize_body(const router_entry* restrict re, const app_context* restrict c, http_request* req, RStruct** out_rs) {
    // Crefl uses null term so we have to null terminate it.
    memcpy(STR_SCRATCH, re->deserialize_typename.data, re->deserialize_typename.len);
    STR_SCRATCH[re->deserialize_typename.len] = '\0';
    // Search the type in the scope.
    RStruct* s = refl_fetch_struct_from(c->crefl_data->c_handle, STR_SCRATCH);
    if (!s) {
        // The app has provided an invalid name
        mlog_error("Couldn't find reflected type: %s for deserialization.", STR_SCRATCH);
        return HTTP_INTERNAL_SERVER_ERROR;
    }
    if (req->body.len == 0) return HTTP_BAD_REQUEST;
    DeserializeResult dr = {0};
    switch (re->expect)
    {
    case CONTENT_JSON:
        dr = deserialize_json(s, req->body.data, req->body.len);
        break;
    
    default:
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    if (dr.st == DR_ERR) return HTTP_UNPROCESABLE_CONTENT;
    req->decoded.data = dr.data.buff;
    req->decoded.capacity = req->decoded.count = s->total_size;
    if (out_rs) *out_rs = s;
    return HTTP_OK;
}

typedef struct {
   char* name;
   uint8_t age;
   double height;
} user_create_payload;

static inline void check_and_free_deserialized(RStruct* s, u8_vec* decoded) {
    if (!s) { 
        mlog_error("Couldn't get the metadata to free the deserialized data.");
        return;
    }
    if (!decoded->data) return;
    refl_generic_destructor(s, decoded->data);
}

#define MSG_MAX_LEN 4096

/**
 * @brief mock route protocol <METHOD> <ROUTE> [<body>] -> GET /about, POST /getuser {"id":2}
 * TEMP client_handler
 */
async(client_handler, int fd) {
    detach;
    int fd = clean_on_drop(arg(fd));

    // Buffer to get the user response.
    // This buffer will be reuse when sending the response back if posible.
    char __b[MSG_MAX_LEN];
    u8_vec buff = {.data = (uint8_t*)__b, .capacity = MSG_MAX_LEN, .count = 0};
    
    // Hot reloading: get the current version and get the most recent router.
    size_t version = APP_CURR_VERSION;
    app_context* c = version_get_ctx(version);
    if (!c) { 
        send_response_back_host_error(fd, HTTP_INTERNAL_SERVER_ERROR, &buff);
        task_return_void();
    }
    // Increase reference count the app ctx.
    c->ref_count++;
    router* rout = &c->router;
    

    ssize_t sz = io_timeout(ssize_t, asc_recv(fd, buff.data, buff.capacity), 600000, {
        mlog_warn("<%d> timed out", fd);
        task_return_void();
    });

    if (sz < 0) {
        mlog_error("Couldn't receive from client: %s", strerror(errno));
        goto __exit_premature;
    } else if (sz == 0) {
        send_response_back_host_error(fd, HTTP_BAD_REQUEST, &buff);
        goto __exit_premature;
    }

    buff.count = sz;
    //mlog_info("Client sent: %.*s", U8_VEC_FMT(buff));

    http_request req = (http_request){0};

    // Parse input buffer
    routing_result r = http_mock_route_parser(&buff, &req);
    if (r.st == MCFW_ERR) {
        send_response_back_host_error(fd, r.data.err, &buff);
        goto __exit_premature;
    }

    // Try to find handler
    r = router_get_entry(rout, &req);
    if (r.st == MCFW_ERR) {
        send_response_back_host_error(fd, r.data.err, &buff);
        goto __exit_premature;
    }

    router_entry* re = (router_entry*)r.data.ok_data;

    if (re->method_fptr == NULL) {
        mlog_error("The handler responsible for route \"%.*s\" if a NULL pointer.", U8_SLICE_FMT(re->route_str));
        send_response_back_host_error(fd, HTTP_INTERNAL_SERVER_ERROR, &buff);
        goto __exit_premature;
    }

    
    // Get the reflected struct for destructing the data.
    RStruct* deser_type = NULL;
    int needs_deserialization = req_deserializing_lut[re->expect];

    // Deserializing if its required.
    if (needs_deserialization && re->r_kind >= PAYLOAD_METHODS_IDX) {
        http_status_code code = try_deserialize_body(re, c, &req, &deser_type);
        if (code != HTTP_OK) {
            send_response_back_host_error(fd, code, &buff);
            goto __exit_premature;
        }
    }


    struct method_args a = {0, req};
    http_response* res =  await_ret(http_response*, spawn_task_on_fptr(re->method_fptr, &a));

    mlog_info("a");

    // Error while processing the response
    if (!res) { 
        send_response_back_host_error(fd, HTTP_INTERNAL_SERVER_ERROR, &buff);
        mlog_error("null ptr received from method: %s %.*s", 
            refl_enum_get_label((REnum*)CACHED_ENUM_REQ_KIND, req.r_kind), U8_SLICE_FMT(req.path));
        goto __exit;
    } else if (res->signature != HTTP_RES_SIGNATURE) {
        send_response_back_host_error(fd, HTTP_INTERNAL_SERVER_ERROR, &buff);

        mlog_error("http response received from method: %s %.*s is garbage",
            refl_enum_get_label((REnum*)CACHED_ENUM_REQ_KIND, req.r_kind), U8_SLICE_FMT(req.path));

        goto __exit;
    }

    mlog_info("a");
    
    RStruct* ser_type = NULL;
    

    // Serialize response if needed
    if (req_serializing_lut[res->type]) {
        u8_vec new_buff = U8_VEC_NULL;
        u8_vec old_buff = res->buff;
        http_status_code code = try_serialize_response(c, res, &new_buff, &ser_type);
        res->buff = new_buff;
        if (code != HTTP_OK) {
            U8_VEC_FREE(old_buff);
            free(res);
            goto __exit;
        }
        refl_generic_destructor(ser_type, old_buff.data);
    }

    mlog_info("a");

    // Send handler's response back
    send_response_back(fd, &buff, res);

    // Freeings
    U8_VEC_FREE(res->buff); // Needs to be NULL if no response body.
    free(res);

__exit:
    if (needs_deserialization) check_and_free_deserialized(deser_type, &req.decoded);
__exit_premature:
    mlog_info("a");
    c->ref_count--;
    if (c->ref_count == 0 && APP_CURR_VERSION != version) {
        mlog_info("called");
        version_drop_ctx(c, 1); }
    end_return_void();
}

async(accept_loop, tcp_listener_t* t) {
    tcp_listener_t* t = arg(t);
    uint64_t shutdown_payload;
    oneshot_pair_t graceful_shutdown = oneshot_new(sizeof(uint64_t), 0);
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
                mlog_warn("Triggered server graceful shutdown, payload: 0x%lX", shutdown_payload);
                break;
            }
        );
        if (client_fd >= 0) {
            task_spawn(client_handler, .fd = client_fd);
        }
    }
    mlog_info("bef drop");
    oneshot_drop_rx((oneshot_rx_t)graceful_shutdown);
    mlog_info("aft drop");
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

async(server_init) {
    uint8_t addr[4];
    uint16_t port = APP_CONFIG_STRUCT->host.port;
    memcpy(addr, &APP_CONFIG_STRUCT->host.addr, 4);

    // Let the reflection engine work on this thread
    refl_threadlocal_init(0);

    // Load the cached reflected types
    CACHED_ENUM_REQ_KIND = refl_fetch_enum("request_kind");
    CACHED_ENUM_STR_STATUS = refl_fetch_enum("http_status_code");


    tcp_listener_t* t = clean_on_drop(asc_tcp_bind(addr, port));
    if (!t) {
        mlog_panic("couldn't create tcp listener: %s", strerror(errno));
        task_return(int, 1);
    }

    load_app();
    task_handle_t t_watcher;
    if (ARG_FLAGS.hr_enabled) t_watcher = task_spawn(version_watch_async);
    await(task_spawn(accept_loop, .t = t));
    mlog_info("returned");
    if (ARG_FLAGS.hr_enabled) drop_task(t_watcher);
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