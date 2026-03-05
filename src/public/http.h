#ifndef PUBLIC_HTTP_H
#define PUBLIC_HTTP_H

#include "types.h"


REFLECT(enum) {
    HTTP_OK = 200,
    HTTP_BAD_REQUEST = 400,
    HTTP_FORBIDDEN = 403,
    HTTP_NOT_FOUND = 404,
    HTTP_METHOD_NOT_ALLOWED = 405,
    HTTP_REQUEST_TIMEOUT = 408,
    HTTP_GONE = 410,
    HTTP_LENGTH_REQUIRED = 411,
    HTTP_CONTENT_TOO_LARGE = 413,
    HTTP_UNSUPORTED_MEDIA_TYPE = 415,
    HTTP_UNPROCESABLE_CONTENT = 422,
    HTTP_LOCKED = 423,
    HTTP_HEADERS_TOO_LARGE =  431,
    HTTP_INTERNAL_SERVER_ERROR = 500,
    HTTP_NOT_IMPLEMENTED = 501,
    HTTP_SERVICE_UNAVAILABLE = 503
} http_status_code;

// Number of (valid) enum members.
#define CONTENT_TYPE_N_MEMB 8

typedef enum {
    // Value when the MIME media type isn't represented in this enum.
    // It will be available to lookup as string slice in the miscellaneous headers.
    CONTENT_OTHER = -2,
    // When especified header is missing.
    CONTENT_NOT_SPECIFIED = -1,
    CONTENT_TEXT,
    CONTENT_HTML,
    CONTENT_JAVASCRIPT,
    CONTENT_PNG,
    CONTENT_JPEG,
    CONTENT_ICO,
    CONTENT_JSON,
    CONTENT_BINARY
} http_content_type;

#define PAYLOAD_METHODS_IDX 3

REFLECT(enum) {
    GET,
    DELETE,
    HEAD,
    POST = 3,
    PUT,
    PATCH,
    OPTIONS
} request_method;

typedef enum {
    ENCODING_IDENTITY = 0, // No compression
    ENCODING_GZIP,
    ENCODING_DEFLATE,
    ENCODING_BR
} http_compression;

typedef enum {
    TRANSFER_NOT_SPECIFIED = -1,
    TRANSFER_INMEDIATE, // Content-Length: ----
    TRANSFER_CHUNKED,   // Transfer-Enconding: chunked
    TRANSFER_FILE       // Internal state
} transmission_encoding;

struct http_header {
    u8_slice header_name;
    u8_slice val;
};

typedef struct {
    struct http_header* table;
    size_t n_headers;
    size_t capacity;
} http_header_table;

struct http_core_headers {
    bool conn_keep_alive;
    http_content_type type;
    http_content_type content_type;
    transmission_encoding tx_encoding;
    union {
        size_t content_length;
        size_t max_body_length;
    } payload_len;
    u8_slice host;
    u8_slice accept;
};


typedef struct __MCFW_HTTP_REQUEST__ {
    int minor_version;
    request_method r_method;
    u8_slice path;

    // string view of the body inside the full request -> If no body pointer is NULL!!
    u8_slice body;
    /**
     * @brief heap allocated deserialized data extracted from the request: ex. deserialized struct from a JSON POST.
     * It will be NULL if there's nothing to decode. 
     * @attention This buffer is freed automatically, don't try to free it.
    */ 
    u8_vec deserialized;

    struct http_core_headers core_headers;
    http_header_table misc_headers;
} http_request;

typedef struct __MCFW_HTTP_RESPONSE__ http_response;

http_response* http_response_create(http_status_code c, http_content_type res_type, u8_slice mime_type, char* __type, u8_vec buff);


#define RES(_code, _content_type, _u8_vec) http_response_create((_code), (_content_type), (u8_slice){0}, NULL, (_u8_vec))
#define RES_OK(_content_type, _u8_vec) http_response_create(HTTP_OK, (_content_type), (u8_slice){0}, NULL, (_u8_vec))
#define RES_EMPTY(_code) http_response_create((_code), CONTENT_NOT_SPECIFIED, (u8_slice){0}, NULL, (u8_vec){0})
#define RES_OTHER(_code, _mime_type_string, _u8_vec) http_response_create((_code), CONTENT_OTHER, SLICE_CSTR(_mime_type_string), NULL, (_u8_vec))

#define RES_SERIALIZE(_code, _content_type, _type, _u8_vec) ({\
    http_response_create(\
        _code, CONTENT_JSON, (u8_slice){0}, #_type, (_u8_vec));\
})

#define RES_SERIALIZE_OK(_content_type, _type, _u8_vec) RES_SERIALIZE(HTTP_OK, _content_type, _type, _u8_vec)


typedef enum {
    HTTP_FLAG_CLOSE_CONN            = 1,
    HTTP_FLAG_DEALLOC_BODY          = 1 << 1,
    HTTP_FLAG_DEEP_DEALLOC          = 1 << 2,   
    HTTP_FLAG_TRANSFER_INMEDIATE    = 1 << 3, //default  // TODO: transfer methods
    HTTP_FLAG_TRANSFER_CHUNKED      = 1 << 4,
    HTTP_FLAG_TRANSFER_FILE         = 1 << 5
} http_response_flag_mask;

// Response flags
http_response* http_res_set_flag(http_response* r, http_response_flag_mask m, int val);

#define RES_F_CLOSE(_res_expr) http_res_set_flag((_res_expr), HTTP_FLAG_CLOSE_CONN, 1);
#define RES_F_DEALLOC(_res_expr) http_res_set_flag((_res_expr), HTTP_FLAG_DEALLOC_BODY, 1);
#define RES_F_DEEP(_res_expr) http_res_set_flag((_res_expr), HTTP_FLAG_DEEP_DEALLOC, 1);




/**
 * @brief hash table lookup for a name in the headers table.
 * @return table entry with the name and the value or NULL string slice if not found.
 */
u8_slice http_get_header_val(u8_slice name, http_header_table* headers);

#endif // PUBLIC_HTTP_H
