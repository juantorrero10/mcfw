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

#include "vendor/picohttpparser.h"

#include "public/types.h"
#include "public/router.h"
#include "public/http.h"
#include "public/resources.h"

#include "private/http.h"
#include "private/router.h"
#include "private/version.h"
#include "private/glob.h"
#include "private/log.h"


#define TOTAL_CLIENTS g_n_clients
static uint64_t TOTAL_CLIENTS = 0;

static oneshot_tx_ref_t g_shutdown = NULL;

oneshot_tx_ref_t resources_graceful_shutdown() { return g_shutdown; }
uint64_t* resources_total_clients() { return &TOTAL_CLIENTS; }

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
 * @brief Send a bodyless error back to the client.
 * This is when 500's, 404's, 405's and simple 400's are sent.
 * @attention Assumption is made that the error response fits inside the buffer.
 */
void send_response_back_host_error(int fd, int code, u8_vec* buffer) {
    const char* str_status = refl_enum_get_label((REnum*)CACHED_ENUM_STR_STATUS, code);
    if (!str_status) str_status = "INTERNAL_SERVER_ERROR";

    char* fmt_template = 
        "HTTP/1.1 %03d %s\r\n"
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

void send_response_back(int fd, u8_vec* buffer, http_response* res, int keep_alive) {
    // Get content type header
    char* ctype_str = (res->type > 0 && res->type < CONTENT_TYPE_N_MEMB)? str_response_type_lut[res->type] : NULL;
    if (ctype_str == NULL) ctype_str = "application/octet-stream";

    // Get string version of the error code.
    const char* str_status = refl_enum_get_label((REnum*)CACHED_ENUM_STR_STATUS, res->code);
    if (!str_status) str_status = "OK"; // Fallback case.

    char date_buff[32];


    // TODO: vec push

    char* fmt_template = 
        "HTTP/1.1 %03d %s\r\n"
        "Server: "SERVER_HEADER_VAL"\r\n"
        "Date: %s\r\n"
        "Connection: %s\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "\r\n"
        ;
    buffer->count = snprintf((char*)buffer->data, buffer->capacity, fmt_template, 
        res->code, STRIP_HTTP_PREFIX(str_status), get_gmt_date(date_buff, 32), (keep_alive)? "keep-alive" : "close",
        ctype_str, res->buff.count);

    // If somehow headers get too heavy and exceed HEADER_MAX_SIZE - 2 (trailing \r\n)
    if (buffer->count >= buffer->capacity - 2) {
        send_response_back_host_error(fd, HTTP_INTERNAL_SERVER_ERROR, buffer);
        return;
    }


    struct iovec vecs[2] = {
        // Headers
        {.iov_base = buffer->data, .iov_len = buffer->count},
        // Body
        {.iov_base = res->buff.data, .iov_len = res->buff.count}
    };


    struct msghdr full_msg = {
        .msg_iov = vecs, .msg_iovlen = 2
    };

    ssize_t sz = io_timeout(ssize_t, asc_sendmsg(fd, &full_msg), SEND_TIMEOUT, {
        mlog_warn("<%d> timed out", fd);
        return;
    });
    if (sz < 0) {
        mlog_warn("couldn't send to client: %s", strerror(errno));
    }
}

http_status_code try_serialize_response(const app_context* c, http_response* res, u8_vec* out_buff, RStruct** out_rs) {
    RStruct* s = refl_fetch_struct_from(c->crefl_data->c_handle, res->serialize_typename);
    if (!s) {
        // The app has provided an invalid name
        mlog_error("Couldn't find reflected type: %s for serialization.", res->serialize_typename);
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
    req->deserialized.data = dr.data.buff;
    req->deserialized.capacity = req->deserialized.count = s->total_size;
    if (out_rs) *out_rs = s;
    return HTTP_OK;
}


static inline void check_and_free_deserialized(RStruct* s, u8_vec* deserialized) {
    if (!s) { 
        mlog_error("Couldn't get the metadata to free the deserialized data.");
        return;
    }
    if (!deserialized->data) return;
    refl_generic_destructor(s, deserialized->data);
}


static int _get_req_from_str(u8_slice* method) {
    REnum* e = (REnum*)CACHED_ENUM_REQ_KIND;
    for (size_t i = 0; i < e->n_elements; i++)
    {
        size_t method_len = strlen(e->names[i]);
        if (method_len != method->len) continue;
        if (!memcmp(method->data, e->names[i], method->len)) return e->indexes[i];
    }
    
    return -1;
}

/**
 * @brief Entry point for every tcp connection.
 */
async(client_handler, int fd) {
    detach;
    int fd = clean_on_drop(arg(fd));
    // Global resources
    TOTAL_CLIENTS++;

    int close_connection = 0;

    // Buffer to get the user response.
    // This buffer will be reuse when sending the response back if posible.
    char __b[MAX_HEADER_SIZE];
    u8_vec buff = VEC_FROM(__b, 0, MAX_HEADER_SIZE);

    // Pico headers buffer
    struct phr_header phr_headers[PHR_HEADER_BUFF_SIZE];
    
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

    ssize_t sz = 0;

__recv_begin:
    

    sz = io_timeout(ssize_t, asc_recv(fd, buff.data, buff.capacity), 3000, {
        send_response_back_host_error(fd, HTTP_REQUEST_TIMEOUT, &buff);
        goto __exit_premature;
    });
    if (sz < 0) {
        mlog_error("Couldn't receive from client <%d>: %s", fd, strerror(errno));
        goto __exit_premature;
    } else if (sz == 0) {
        send_response_back_host_error(fd, HTTP_BAD_REQUEST, &buff);
        goto __exit_premature;
    }


    // Build request struct
    http_request req = (http_request){0}; 
    req.misc_headers.n_headers = PHR_HEADER_BUFF_SIZE;
    buff.count = sz;
    u8_slice method = {0};

    // ret -> bytes consumed or error.
    int parse_result = phr_parse_request(
        (const char*)buff.data, 
        buff.count, 
        (const char**)&method.data, 
        &method.len, 
        (const char**)&req.path.data, 
        &req.path.len, 
        &req.minor_version,
        phr_headers, 
        &req.misc_headers.n_headers,
        0);


    if (parse_result == -1) {
        send_response_back_host_error(fd, HTTP_BAD_REQUEST, &buff);
        goto __exit_premature;
    }


    if (parse_result == -2) {
        send_response_back_host_error(fd, HTTP_HEADERS_TOO_LARGE, &buff);
        goto __exit_premature;
    }

    /**
     * TEMP until correctly handling the body
     */
    if ((size_t)sz == buff.capacity) {
        send_response_back_host_error(fd, HTTP_CONTENT_TOO_LARGE, &buff);
        goto __exit_premature;
    } 

    // Set the str view of the body if there is one
    if ((size_t)parse_result < buff.count) {
        req.body.data = buff.data + (size_t)parse_result;
        // Set the len later.
    }

    struct http_header misc_headers[PHR_HEADER_BUFF_SIZE * 2];
    // Since we are dealing with external stacks i don't trust it to be 0-initialized.
    memset(misc_headers, 0, sizeof(misc_headers));

    http_create_misc_headers_hashtable(phr_headers, misc_headers, PHR_HEADER_BUFF_SIZE * 2);

    // Set the method and search for the handler
    req.r_method = (request_method)_get_req_from_str(&method);
    routing_result r = router_get_entry(rout, &req);
    if (r.st == MCFW_ERR) {
        send_response_back_host_error(fd, r.data.err, &buff);
        goto __exit_premature;
    }

    // The handler for the request.
    router_entry* re = (router_entry*)r.data.ok_data;

    
    req.misc_headers.table = misc_headers;
    req.misc_headers.capacity = PHR_HEADER_BUFF_SIZE * 2;
    http_status_code code = http_check_core_headers(&req, re);
    if (code != HTTP_OK) {
        send_response_back_host_error(fd, code, &buff);
        goto __exit_premature;
    }


    if (re->method_fptr == NULL) {
        mlog_error("The handler responsible for route \"%.*s\" if a NULL pointer.", SLICE_FMT(re->route_str));
        send_response_back_host_error(fd, HTTP_INTERNAL_SERVER_ERROR, &buff);
        goto __exit_premature;
    }

    
    // Get the reflected struct for destructing the data.
    RStruct* deser_type = NULL;
    int needs_deserialization = req_deserializing_lut[re->expect];

    // Deserializing if its required.
    if (needs_deserialization && re->r_method >= PAYLOAD_METHODS_IDX) {
        http_status_code code = try_deserialize_body(re, c, &req, &deser_type);
        if (code != HTTP_OK) {
            send_response_back_host_error(fd, code, &buff);
            goto __exit_premature;
        }
    }


    if (TOTAL_CLIENTS > 1) yield;
    // Call handler.
    http_response* res =  re->method_fptr(&req);
    

    // Error while processing the response
    if (!res) { 
        send_response_back_host_error(fd, HTTP_INTERNAL_SERVER_ERROR, &buff);
        mlog_error("null ptr received from method: %s %.*s", 
            refl_enum_get_label((REnum*)CACHED_ENUM_REQ_KIND, req.r_method), SLICE_FMT(req.path));
        goto __exit;
    } else if (res->signature != HTTP_RES_SIGNATURE) {
        send_response_back_host_error(fd, HTTP_INTERNAL_SERVER_ERROR, &buff);

        mlog_error("http response received from method: %s %.*s is garbage",
            refl_enum_get_label((REnum*)CACHED_ENUM_REQ_KIND, req.r_method), SLICE_FMT(req.path));

        goto __exit;
    }
    
    RStruct* ser_type = NULL;
    int swapped = 0;

    // Serialize response if needed
    if (res->serialize) {
        // Swap buffers
        u8_vec new_buff = VEC_NULL;
        u8_vec old_buff = res->buff;
        http_status_code code = try_serialize_response(c, res, &new_buff, &ser_type);
        res->buff = new_buff;
        if (code != HTTP_OK) {
            if (res->flags & HTTP_FLAG_DEALLOC_BODY) VEC_FREE(old_buff);
            free(res);
            goto __exit;
        }
        if (res->flags & HTTP_FLAG_DEEP_DEALLOC) refl_generic_destructor(ser_type, old_buff.data);
        else if (res->flags & HTTP_FLAG_DEALLOC_BODY) VEC_FREE(old_buff);
        swapped = 1;
    }

    close_connection = !(req.core_headers.conn_keep_alive && !(res->flags & HTTP_FLAG_CLOSE_CONN));
    // Send handler's response back
    send_response_back(fd, &buff, res, !close_connection);

    // Freeings
    if (swapped || res->flags & HTTP_FLAG_DEALLOC_BODY) VEC_FREE(res->buff);
    free(res);

    // Get back the begining if we are not closing the connection
    if (!close_connection) {
        if (needs_deserialization) check_and_free_deserialized(deser_type, &req.deserialized);
        goto __recv_begin; 
    }


__exit:
    if (needs_deserialization) check_and_free_deserialized(deser_type, &req.deserialized);
__exit_premature:
    c->ref_count--;
    if (c->ref_count == 0 && APP_CURR_VERSION != version) {
        version_drop_ctx(c, 1); }
    TOTAL_CLIENTS--;
    end_return_void();
}

async(accept_loop, tcp_listener_t* t) {
    tcp_listener_t* t = arg(t);
    uint64_t shutdown_payload;
    oneshot_pair_t graceful_shutdown = oneshot_new(sizeof(uint64_t), 1);
    mlog_info("Listening on %hhu.%hhu.%hhu.%hhu:%hu", 
        t->ip_addr[0],
        t->ip_addr[1],
        t->ip_addr[2],
        t->ip_addr[3],
        t->port
    );
    g_shutdown = oneshot_move_tx(graceful_shutdown);
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
    oneshot_drop_rx((oneshot_rx_t)graceful_shutdown);
    oneshot_assert_closed(graceful_shutdown);
    end_return_void();
}
/*
static task_handle_t spawn_task_on_fptr(async_fn fptr, void* arg) {


    task__arg__constructor te =  {
        .arg_malloced = 0,
        .arg_size = sizeof(void*),
        .fptr = *fptr,
        .n_pages = TASK_DEFAULT_PAGES,
        .value = arg
    };
    return __task_create(te);
}*/

async(server_init) {
    uint8_t addr[4];
    uint16_t port = APP_CONFIG_STRUCT->host.port;
    memcpy(addr, &APP_CONFIG_STRUCT->host.addr, 4);

    // Let the reflection engine work on this thread
    refl_threadlocal_init(1);

    // Load the cached reflected types
    CACHED_ENUM_REQ_KIND = refl_fetch_enum("request_method");
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