#include <private/mcfw.h>


#define MIN(a, b) ((a) < (b))? (a) : (b)

static inline int _get_req_from_str(DStatic* method) {
    REnum* e = (REnum*)CACHED_ENUM_REQ_KIND;
    for (size_t i = 0; i < e->n_elements; i++)
    {
        size_t method_len = strlen(e->names[i]);
        if (method_len != method->count) continue;
        if (!memcmp(method->s_ptr, e->names[i], method->count)) return e->indexes[i];
    }
    return -1;
}

static inline uint32_t dbj2_hash(DStatic* str)
{
    unsigned long hash = 5381;
    size_t cur = 0; // "cursor"
    uint32_t c;

    while (cur < str->count) {
        c = ((char*)str->s_ptr)[cur++];
        if (c >= 'A' && c <= 'Z') c |= 0x20; // lowercase
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash;
}


/**
 * @brief Create header hashtable.
 */
static void _create_headers_hashtable(HttpRequest* req, struct phr_header* headers, size_t amount) {
    struct HttpHeaderHTable* ht = &req->header_table;
    for (size_t i = 0; i < amount; i++)
    {
        DStatic k = {.s_ptr = (uint8_t*)headers[i].name, .count = headers[i].name_len};
        DStatic v = {.s_ptr = (uint8_t*)headers[i].value, .count = headers[i].value_len};
        size_t hashed_idx = dbj2_hash(&k) % ht->capacity;
        // Collision -> find next empty spot (linear probe)
        while (ht->table[hashed_idx].header_name.s_ptr != NULL) hashed_idx = (hashed_idx + 1) % ht->capacity;
        ht->table[hashed_idx].header_name = k;
        ht->table[hashed_idx].val = v;
        ht->n_headers++;
    }
    req->header_table = *ht;
}


HttpStatusCode req_fill_core_headers(HttpRequest* restrict req, HttpContentType expected) {
    // Connection
    DStatic connection = req_get_header_value(req, DS_LIT("Connection"));
    // No connection header or malformed -> keep-alive. (HTTP 1.1+)
    req->core_headers.conn_keep_alive = 
    (   
        req->minor_version > 0 && (
        !  connection.s_ptr 
        || connection.count != sizeof("close") - 1 
        || strncasecmp((char*)connection.s_ptr, "close", sizeof("close") - 1))
    )? true : false;

    // Accept, the app will deal with the possible NULL.
    req->core_headers.accept = req_get_header_value(req, DS_LIT("Accept"));
    // Host, mandatory in HTTP 1.1+
    req->core_headers.host = req_get_header_value(req, DS_LIT("Host"));
    if (req->minor_version >= 1 && !DS_IS_VALID(req->core_headers.host)) {
        mlog_info("1");
        return HTTP_BAD_REQUEST;
    }

    // Content-Length
    req->core_headers.tx_encoding = TRANSFER_INMEDIATE;
    DStatic content_length = req_get_header_value(req, DS_LIT("Content-Length"));
    req->core_headers.payload_len.content_length = 0;

    if (DS_IS_VALID(content_length)) {
        char num_buff[32];
        req->core_headers.tx_encoding = TRANSFER_INMEDIATE;
        memcpy(num_buff, content_length.s_ptr, MIN(content_length.count, 31));
        num_buff[31] = '\0'; 
        errno = 0; 
        req->core_headers.payload_len.content_length = strtoull(num_buff, NULL, 10);
        if (errno != 0) {
            mlog_info("2");
            return HTTP_BAD_REQUEST;
        }
    }


    // Default value.
    req->core_headers.content_type = CONTENT_NOT_SPECIFIED;

    // Check Content-Length val and Content-type
    // for POST PUT PATCH and OPTIONS
    if (req->r_method >= PAYLOAD_METHODS_IDX) {
        if (req->core_headers.tx_encoding == TRANSFER_NOT_SPECIFIED) return HTTP_LENGTH_REQUIRED;
        if (req->core_headers.payload_len.content_length > MAX_BODY_SIZE) return HTTP_CONTENT_TOO_LARGE;

        req->content.body.capacity = req->core_headers.payload_len.content_length;

        DStatic content_type = req_get_header_value(req, DS_LIT("Content-Type"));
        if (content_type.s_ptr) {
            const struct mime_str_map* m = mime_str_lookup((char*)content_type.s_ptr, content_type.count);
            req->core_headers.content_type = m? (HttpContentType)m->id : CONTENT_OTHER;
        }
        // Validate type
        if (expected != CONTENT_NOT_SPECIFIED && expected != req->core_headers.content_type) {
            return HTTP_UNSUPORTED_MEDIA_TYPE;
        }
    }
    // for GET HEAD DELETE 
    else {
        // Don't allow sending bodies in non payload methods.
        if (req->core_headers.tx_encoding == TRANSFER_CHUNKED ||
            (content_length.s_ptr && req->core_headers.payload_len.content_length > 0)) {
                mlog_info("3");
                return HTTP_BAD_REQUEST;
        }
    }

    return HTTP_OK;
}

HttpStatusCode req_recv_headers(
    Arena* arena,
    int client_fd,
    HttpRequest* restrict req,
    _Out_ DStatic* restrict headers_buff,
    _Out_ DStatic* restrict unused_data,
    _Out_ bool* eof
) {
    // Expand arena to 16Kb (arena may include unused data from a previous request)
    size_t prev = arena->used;
    (void)mcfw_arena_alloc(arena, MAX_HEADERS_SIZE - arena->used, false);
    *headers_buff = (DStatic){
        .s_ptr = arena->p,
        .count = prev,
        .capacity = MIN(MAX_HEADERS_SIZE, arena->alloc_size),
    };

    struct phr_header temp_phr_headers[MAX_REQ_HEADER_COUNT];
    memset(temp_phr_headers, 0, sizeof(struct phr_header) * MAX_REQ_HEADER_COUNT);

    time_t curr_time = time(NULL);
    ssize_t previous_count = 0;

    if (headers_buff->count > 0) {
        goto __parse;
    }

__recv_att:
    void* dst = headers_buff->s_ptr + headers_buff->count;
    size_t ask_for = MIN(RECV_ATT_SIZE, headers_buff->capacity - headers_buff->count);
    // Try to receive headers
    ssize_t sz = io_timeout(
        ssize_t, 
        asc_recv(client_fd, dst, ask_for), 
        RECV_HEADER_TIMEOUT, 
        {
            return HTTP_REQUEST_TIMEOUT;
        }
    );

    if (sz < 0) { 
        int e = errno;
        mlog_error("Counldn't receive from client: <%d>: %s", client_fd, strerror(e));
        return HTTP_INTERNAL_SERVER_ERROR;
    }
    else if (sz == 0) {
        *eof = true;
        return HTTP_NO_CONTENT;
    }
    previous_count = headers_buff->count;
    headers_buff->count += sz;

    // Check for total timeout
    if (time(NULL) - curr_time > TOTAL_HEADER_TIMEOUT) {
        return HTTP_REQUEST_TIMEOUT;
    }

__parse:
    DStatic method = DS_EMPTY();
    size_t temp_phr_len = sizeof(temp_phr_headers) / sizeof(struct phr_header);
    int parse_result = phr_parse_request(
        (const char*)headers_buff->s_ptr, 
        headers_buff->count, 
        (const char**)&method.s_ptr, 
        &method.count, 
        (const char**)&req->path.s_ptr, 
        &req->path.count, 
        &req->minor_version,
        temp_phr_headers, 
        &temp_phr_len,
        previous_count
    );
    method.capacity = method.count;
    req->path.capacity = req->path.count;
    
    // Bad headers
    if (parse_result == -1) {
        mlog_warn("Bad headers from <%d>: %.*s", client_fd, DS_ARGS(*headers_buff));
        return HTTP_BAD_REQUEST;
    }
    // Incomplete headers
    if (parse_result == -2) {
        if (headers_buff->count < headers_buff->capacity) goto __recv_att;
        return HTTP_HEADERS_TOO_LARGE;
    }

    // Create headers hashtable
    DStatic ht_buff = mcfw_arena_alloc(
        arena, 
        sizeof(struct HttpHeader) 
            * HEADER_HASHTABLE_CAPACITY, 
        true
        );

    memset(ht_buff.s_ptr, 0, ht_buff.capacity);

    // Attach to request
    req->header_table = (struct HttpHeaderHTable){
        .table = ht_buff.s_ptr,
        .n_headers = 0,
        .capacity = HEADER_HASHTABLE_CAPACITY
    };

    _create_headers_hashtable(req, temp_phr_headers, temp_phr_len);


    req->r_method = _get_req_from_str(&method);

    // fill unused_data with the extra read bytes
    if ((size_t)parse_result == headers_buff->count) {
        *unused_data = DS_EMPTY();
    } else {
        *unused_data = (DStatic){
            .s_ptr = headers_buff->s_ptr + parse_result,
            .count = headers_buff->count - parse_result,
            .capacity = headers_buff->capacity - parse_result
        };
    }

    // Trim headers_buff count so it strictly represents just the headers
    headers_buff->count = parse_result;

    if (headers_buff->count == 0) {
        *eof = true;
        return HTTP_NO_CONTENT;
    }

    return HTTP_OK;
}


HttpStatusCode req_recv_body(
    int client_fd,
    HttpRequest* restrict req,
    AppContext* restrict app,
    DStatic crefl_type_name,
    const DStatic* restrict unused_data,
    bool deserialize
) {
    req->content.body = DH_EMPTY();
    if(req->core_headers.tx_encoding != TRANSFER_INMEDIATE) {
        mlog_error("Only inmediate is supported.");
        req->core_headers.tx_encoding = TRANSFER_INMEDIATE;
        return HTTP_INTERNAL_SERVER_ERROR;
    }
    size_t content_length = req->core_headers.payload_len.content_length;
    if (content_length > MAX_BODY_SIZE) {
        return HTTP_CONTENT_TOO_LARGE;
    }
    // Check for Expect: 100-continue
    DStatic expect = req_get_header_value(req, DS_LIT("Expect"));
    if (
        DS_IS_VALID(expect)
        && expect.count == sizeof("100-continue") - 1 
        && !strncasecmp((char*)expect.s_ptr, "100-continue", sizeof("100-continue") - 1)) 
    {
        return HTTP_CONTINUE;
    }

    // Allocate body buffer and copy extra data 
    DHeap body = DH_NEW(content_length);
    req->content.body = body;
    if (DS_IS_VALID(*unused_data)) {
        dheap_concat(&body, unused_data);
    }
    time_t starting_time = time(NULL);

    while (body.count > body.capacity) {
        void* dst = body.h_ptr + body.count;
        size_t ask_for = MIN(RECV_ATT_SIZE, body.capacity - body.count); 
        ssize_t chunk_sz = io_timeout(ssize_t, asc_recv(client_fd, dst, ask_for), RECV_BODY_TIMEOUT, {
            return HTTP_REQUEST_TIMEOUT;
        });
        if (chunk_sz < 0) {
            mlog_warn("Couldn't receive from client <%d>: %s", client_fd, strerror(errno));
            return HTTP_INTERNAL_SERVER_ERROR;
        }
        body.count += chunk_sz;

        // Check for total timeout
        if (time(NULL) - starting_time > TOTAL_HEADER_TIMEOUT) {
            return HTTP_REQUEST_TIMEOUT;
        }
    }
    req->content.body = body;

    HttpContentType ctype = req->core_headers.content_type; 
    bool needs_deserialization = 
        deserialize &&
        ctype > 0 && ctype < CONTENT_TYPE_N_MEMB && 
        MIME_LUT[ctype].flags 
        & MIME_FLAG_SERDE;

    if (!needs_deserialization) {
        req->was_deserialized = false;
        return HTTP_OK;
    } else if (content_length == 0) {
        return HTTP_UNPROCESABLE_CONTENT;
    }
        
    RStruct* s = refl_fetch_struct_from(
        app->crefl_data->c_handle, 
        make_cstr(crefl_type_name)
    );
    if (!s) {
        mlog_error("Couldn't find reflected type: %s for deserialization.", g_str_scratch);
        return HTTP_INTERNAL_SERVER_ERROR;
    }
    req->content.deserialization.type = s;

    DeserializeResult dr = deserialize_json(s, body.h_ptr, body.count);
    if (dr.st == DR_ERR) {
        mlog_warn("Deserialization failed: %s", dr.data.err_str);
        return HTTP_UNPROCESABLE_CONTENT;
    }
    req->was_deserialized = true;
    req->content.deserialization.instance = dr.data.buff;
    return HTTP_OK;
}




void req_destroy(HttpRequest* req) {
    if (req->was_deserialized) {
        refl_generic_destructor(
            req->content.deserialization.type,
            req->content.deserialization.instance
        );
    }
    if (DH_IS_VALID(req->content.body)) DH_FREE(req->content.body);
    req->content.body = DH_EMPTY();
    req->was_deserialized = false;
}


/**
 * @brief hash table lookup for a name in the headers table.
 * @return table entry with the name and the value or NULL string slice if not found.
 */
_Public_ DStatic req_get_header_value(const HttpRequest* req, DStatic name) {
    size_t n_attempt = 0;
    const struct HttpHeaderHTable* headers = &req->header_table;
    size_t hashed_idx = dbj2_hash(&name) & (headers->capacity - 1);
    while(n_attempt < headers->n_headers) {
        struct HttpHeader* entry = &headers->table[hashed_idx];
        // Linear probe -> stop when we encounter NULL.
        if(entry->header_name.s_ptr == NULL) return (DStatic){0};
        // Check length and then byte by byte str comp.
        if (entry->header_name.count == name.count 
            && !strncasecmp((char*)entry->header_name.s_ptr, (char*)name.s_ptr, name.count)
        )
            return entry->val;
        n_attempt++;
        hashed_idx = (hashed_idx + 1) & (headers->capacity - 1);
    }
    return (DStatic){0};
}

/**
 * @brief wrapper to req->body.deserialized
 * @returns NULL if no deserialization happned. 
 * This probably means the route was badly configured.
 * @note to obtain the instance just cast it 
 * (ex: ```Type* instance = req_get_deserialized_body(req)```)
 * @attention Do not try to free the data
 */
_Public_ void* req_get_deserialized_body(const HttpRequest* req) {
    if (!req->was_deserialized) {
        mlog_warn(
            "%.*s handler is expecting a deserialized body "
            "when no deserialization was configured to happen. Returning NULL",
        DS_ARGS(req->path));
        return NULL;
    }
    return req->content.deserialization.instance;
}