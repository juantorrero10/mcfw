#include <private/mcfw.h>


void res_default(_Out_ HttpResponse* res, int client_fd) {
    res->minor_version = 1;
    res->code = HTTP_OK;
    res->keep_alive = true;
    res->req = NULL;
    res->signature = HTTP_RES_SIGNATURE;
    res->type = CONTENT_NOT_SPECIFIED;
    res->type_string = (DStatic){0};
    sender_default(&res->stream, client_fd);
}


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

static void res_collect_headers(HttpResponse* res) {
    // Push essential stuff
    char date_buff[32];

    DStatic* buff = &res->stream.resp_headers;
    const char* str_status = refl_enum_get_label((REnum*)CACHED_ENUM_STR_STATUS, res->code);
    const char* cont_type_str = (res->type > 0 && res->type < CONTENT_TYPE_N_MEMB)? (char*)MIME_LUT[res->type].full_str : NULL;
    if (cont_type_str == NULL) cont_type_str = MIME_LUT[CONTENT_BINARY].full_str;
    
    DS_WRITE(*buff, 
        "HTTP/1.%d %d %s\r\n"
        "Server: "SERVER_HEADER_VAL"\r\n"
        "Date: %s\r\n"
        "Connection: %s\r\n"
        "Content-Length: %zu\r\n", 
        res->minor_version, res->code, 
        str_status + 5, get_gmt_date(date_buff, 32),
        (res->minor_version && res->keep_alive)? "keep-alive" : "close", 
        res->stream.total_bytes
    );
    if (res->stream.total_bytes > 0) {
        DS_WRITE(*buff, 
            "Content-Type: %s\r\n", cont_type_str
        );
    }
    DStatic crlf = DS_LIT("\r\n");
    DStatic sep = DS_LIT(": ");

    for (size_t i = 0; i < res->response_headers.count; i++)
    {
        struct HttpHeader* h = &res->response_headers.data[i];
        if (!DS_IS_VALID(h->header_name) || !DS_IS_VALID(h->val)) continue;
        if (buff->capacity < 
            buff->count + (2 * crlf.count) + 
            sep.count + h->header_name.count + 
            h->val.count
        ) break;
        dstatic_concat(buff, &h->header_name);
        dstatic_concat(buff, &sep);
        dstatic_concat(buff, &h->val);
        dstatic_concat(buff, &crlf);
    }

    dstatic_concat(buff, &crlf);
    res->stream.total_bytes += buff->count;
}

void res_send(HttpResponse* res, uint64_t timeout_ms) {
    uint8_t __b[MAX_HEADERS_SIZE];
    res->stream.resp_headers = DS_FROM(__b, 0, MAX_HEADERS_SIZE);
    res_collect_headers(res);
    sender_push(&res->stream, timeout_ms);
}

void res_destroy(HttpResponse* res) { sender_destroy(&res->stream); }

void res_send_no_destroy(HttpResponse* res, uint64_t timeout_ms) {
    uint8_t __b[MAX_HEADERS_SIZE];
    res->stream.resp_headers = DS_FROM(__b, 0, MAX_HEADERS_SIZE);
    res_collect_headers(res);
    sender_push(&res->stream, timeout_ms);
}


/**
 * @note this is the default state of the content
 */
_Public_ void res_set_content_empty(HttpResponse* res) {
    res->type = CONTENT_NOT_SPECIFIED;
    sender_default(&res->stream, res->stream.client_fd);
}


_Public_ void __res_set_content(HttpResponse* res, HttpContentType t, DBuffer b) {
    res->type = t;
    sender_configure_inm(&res->stream, b, t);
}


/**
 * @brief Serialize an instance and set it as the content of the response.
 * @param res HttpResponse*
 * @param content_type HttpContentType
 * @param reflected_type name of the typedef struct of the reflected type
 * @param instance void* pointer to var 
 */
_Public_ void __res_set_content_serialize__(HttpResponse* res, HttpContentType t, const char* ctype, const void* instance) {
    AppContext* ctx = server_get_ctx();
    // TODO: remove if more types are allowed on the future
    if (t != CONTENT_JSON) {
        mlog_error("Tried to deserialize unsupported type. Only JSON is supported");
        goto _ser_error;
    }
    RStruct* type = refl_fetch_struct_from(ctx->crefl_data->c_handle, ctype);
    if (!type) {
        mlog_error("Couldn't find reflected info about type '%s'", ctype);
        goto _ser_error;
    }
    DHeap buff_serialized = DH_EMPTY();
    buff_serialized.count = serialize_json(type, (void*)instance, &(buff_serialized.h_ptr), true);
    buff_serialized.capacity = buff_serialized.count;

    res->type = t;
    sender_configure_inm(&res->stream, DB_FROM(buff_serialized), t);

_ser_error:
    res_set_status_code(res, HTTP_INTERNAL_SERVER_ERROR);
    res_set_content_empty(res);
    return;
}

_Public_ void res_set_content_type(HttpResponse* res, DStatic MIME_type_name) {
    // try to find it if or it as a string
    const struct mime_str_map* m = mime_str_lookup(MIME_type_name.s_ptr, MIME_type_name.count);
    if (m) {
        res->type = (HttpContentType)m->id;
        sender_set_mime_flags(&res->stream, res->type);
    }
    res->type_string = MIME_type_name;
    sender_set_mime_flags(&res->stream, CONTENT_OTHER);
}

static inline DStatic get_extension(const DStatic* file_name) {
    uint8_t* ptr = (uint8_t*)memrchr(file_name->s_ptr, '.', file_name->count);
    if (!ptr) return (DStatic){0};
    size_t len = (size_t)file_name->s_ptr - ((size_t)ptr - file_name->count);
    return DS_FROM(ptr, len, len);
}

static inline DStatic get_file_name(const DStatic* file_name) {
    char* ptr = file_name->s_ptr + file_name->count - 1;
    while(ptr != file_name->s_ptr) { 
        if (*ptr == '/') { ptr++; break; }
        ptr--;
    }
    ptrdiff_t d = (ptrdiff_t)ptr - (ptrdiff_t)file_name->s_ptr;
    size_t sz = file_name->count - d;
    return DS_FROM(ptr, sz, sz);
}




/**
 * @brief Send the contents of a file as a response
 * @note HttpContentType will be deduced though file extension. Call ``````
 * @param res HttpResponse*
 * @param downloadable Make the target browser download as a file through the Content-Disposition header
 */
_Public_ void res_set_content_file(
    HttpResponse* res, 
    HttpContentType t,
    DStatic file_path, 
    bool downloadable
) {
    if (!DS_IS_VALID(file_path)) {
        mlog_error("Gotten null ptr as a file path");
        res->code = HTTP_INTERNAL_SERVER_ERROR;
        return;
    }
    if (t == CONTENT_NOT_SPECIFIED) {
        DStatic extension = get_extension(&file_path);
        const struct mime_ext_map* m = mime_ext_lookup(extension.s_ptr, extension.count);
        if (m) {
            t = m->id;
        }
    }
    res->type = t;
    sender_configure_file(&res->stream, file_path, t, req_get_header_value(res->req, DS_LIT("Range")));

    if (downloadable) {
        uint8_t buff[1024];
        DStatic ds = DS_FROM(buff, 0, 1024);
        DStatic fn = get_file_name(&file_path);
        DS_WRITE(ds, "attachment; filename=\"file %.*s\"", DS_ARGS(fn));
        res_add_header(res, DS_LIT("Content-Disposition"), ds);
    }
}

/**
 * @brief Set HTTP status code that will be sent to the client.
 * @note HTTP_OK (200) is the default code, if this code is desired
 * you can omit calling this function
 */
_Public_ void res_set_status_code(HttpResponse* res, HttpStatusCode code) {
    res->code = code;
}

/**
 * @brief Add a header to the HTTP response.
 * @returns true if headers was successfully added. 
 * There is a limit of 128 headers, after which, false is returned
 */
_Public_ bool res_add_header(HttpResponse* res, DStatic name, DStatic value) {
    if (res->response_headers.count >= res->response_headers.capacity) {
        return false;
    }
    res->response_headers.data[res->response_headers.count++] = 
        (struct HttpHeader){
            .header_name = name,
            .val = value
        };
    return true;
}

_Public_ void res_close_connection(HttpResponse* res) {
    res->keep_alive = false;
}