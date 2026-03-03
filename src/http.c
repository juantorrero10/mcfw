#define _GNU_SOURCE

#include <stdlib.h>
#include <errno.h>

#include <crefl/core.h>

#include "vendor/picohttpparser.h"

#include "public/http.h"
#include "private/types.h"
#include "private/http.h"
#include "private/log.h"
#include "private/glob.h"
#include "private/router.h"

#define MIN(a, b) ((a) < (b))? (a) : (b)

// Feels overkill but in the future way may have more formats
const int req_deserializing_lut[CONTENT_TYPE_N_MEMB] = {
    [CONTENT_JSON] = 1,
};

const int req_serializing_lut[CONTENT_TYPE_N_MEMB] = {
    [CONTENT_JSON] = 1,
};


char* str_response_type_lut[CONTENT_TYPE_N_MEMB] = {
    [CONTENT_HTML]       = "text/html",
    [CONTENT_TEXT]       = "text/plain",
    [CONTENT_JAVASCRIPT] = "text/javascript",
    [CONTENT_JSON]       = "application/json",
    [CONTENT_BINARY]     = "application/octet-stream",
    [CONTENT_ICO]        = "image/vnd.microsoft.icon",
    [CONTENT_JPEG]       = "image/jpeg",
    [CONTENT_PNG]        = "image/png"
};

static inline uint32_t dbj2_hash(u8_slice* str)
{
    unsigned long hash = 5381;
    size_t cur = 0; // "cursor"
    uint32_t c;

    while (cur < str->len) {
        c = (char)str->data[cur++];
        if (c >= 'A' && c <= 'Z') c |= 0x20; // lowercase
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash;
}

/**
 * @brief hash table lookup for a name in the headers table.
 * @return table entry with the name and the value or NULL string slice if not found.
 */
u8_slice http_get_header_val(u8_slice name, http_header_table* headers) {
    size_t n_attempt = 0;
    size_t hashed_idx = dbj2_hash(&name) & (headers->capacity - 1);
    while(n_attempt < headers->n_headers) {
        struct http_header* entry = &headers->table[hashed_idx];
        // Linear probe -> stop when we encounter NULL.
        if(entry->header_name.data == NULL) return (u8_slice){0};
        // Check length and then byte by byte str comp.
        if (entry->header_name.len == name.len 
            && !strncasecmp((char*)entry->header_name.data, (char*)name.data, name.len)
        )
            return entry->val;
        n_attempt++;
        hashed_idx = (hashed_idx + 1) & (headers->capacity - 1);
    }
    return (u8_slice){0};
}
/**
 * @brief Create header hashtable.
 */
void http_create_misc_headers_hashtable(struct phr_header* headers, struct http_header* out_headers, size_t buff_size) {
    for (size_t i = 0; i < buff_size; i++)
    {
        u8_slice k = {.data = (uint8_t*)headers[i].name, .len = headers[i].name_len};
        u8_slice v = {.data = (uint8_t*)headers[i].value, .len = headers[i].value_len};
        size_t hashed_idx = dbj2_hash(&k) & (buff_size - 1);
        // Collision -> find next empty spot (linear probe)
        while (out_headers[hashed_idx].header_name.data != NULL) hashed_idx = (hashed_idx + 1) & (buff_size - 1);
        out_headers[hashed_idx].header_name = k;
        out_headers[hashed_idx].val = v;
    }
    
}

/**
 * Generated str to http_content_type enum conversion though gperf. See src/gperf/str_to_content_type.c  
 */
struct mime_type { char* name; http_content_type type; };
struct mime_type *
str_to_content_type (register const char *str, register size_t len);

http_status_code http_check_core_headers(http_request* restrict req, router_entry* restrict re) {
    // Connection
    u8_slice connection = http_get_header_val(U8_CSTR_LIT_SLICE("Connection"), &req->misc_headers);
    // No connection header or malformed -> keep-alive. (HTTP 1.1+)
    req->core_headers.conn_keep_alive = 
    (   
        req->minor_version > 0 && (
        !  connection.data 
        || connection.len != sizeof("close") - 1 
        || strncasecmp((char*)connection.data, "close", sizeof("close") - 1))
    )? true : false;

    // Accept, the app will deal with the possible NULL.
    req->core_headers.accept = http_get_header_val(U8_CSTR_LIT_SLICE("Accept"), &req->misc_headers);
    // Host, mandatory in HTTP 1.1+
    req->core_headers.host = http_get_header_val(U8_CSTR_LIT_SLICE("Host"), &req->misc_headers);
    if (req->minor_version >= 1 && !req->core_headers.host.data) {
        return HTTP_BAD_REQUEST;
    }

    // Content-Length
    u8_slice content_length = http_get_header_val(U8_CSTR_LIT_SLICE("Content-Length"), &req->misc_headers);
    req->core_headers.content_length = 0;

    if (content_length.data) {
        char num_buff[32];
        memcpy(num_buff, content_length.data, MIN(content_length.len, 31));
        errno = 0; 
        req->core_headers.content_length = strtoull(num_buff, NULL, 10);
        if (errno != 0) {
            return HTTP_BAD_REQUEST;
        }
    }


    // Default value.
    req->core_headers.content_type = CONTENT_NOT_SPECIFIED;

    // Check Content-Length val and Content-type
    // for POST PUT PATCH and OPTIONS
    if (req->r_method >= PAYLOAD_METHODS_IDX) {
        if (!content_length.data) return HTTP_LENGTH_REQUIRED;
        if (req->core_headers.content_length > MAX_BODY_SIZE) return HTTP_CONTENT_TOO_LARGE;

        req->body.len = req->core_headers.content_length;

        u8_slice content_type = http_get_header_val(U8_CSTR_LIT_SLICE("Content-Type"), &req->misc_headers);
        if (content_type.data) {
            struct mime_type* m = str_to_content_type((char*)content_type.data, content_type.len);
            req->core_headers.content_type = m? m->type : CONTENT_OTHER;
        }
        // Validate type
        if (re->expect != CONTENT_NOT_SPECIFIED && re->expect != req->core_headers.content_type) {
            return HTTP_UNSUPORTED_MEDIA_TYPE;
        }
    }
    // for GET HEAD DELETE 
    else {
        // Don't allow sending bodies in non payload methods.
        if (req->core_headers.content_length > 0 || req->body.data != NULL) {
            return HTTP_BAD_REQUEST;
        }
    }



    return HTTP_OK;
}


http_response* http_response_create(http_status_code c, http_content_type res_type, char* __type, u8_vec buff) {
    http_response* r = malloc(sizeof(http_response));
    *r = (http_response){0};
    r->typename = __type;
    r->signature = HTTP_RES_SIGNATURE;
    r->buff = buff;
    r->code = c;
    r->type = res_type;
    return r;
}
