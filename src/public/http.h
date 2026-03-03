#ifndef PUBLIC_HTTP_H
#define PUBLIC_HTTP_H

#include "types.h"

#define HTTP_RES_JSON(_code, _type, _instance) ({\
    http_response_create(\
        _code, CONTENT_JSON, #_type, \
        U8_VEC(_instance, sizeof(_type)));\
})
#define HTTP_RES_PLAIN(_code, _u8_vec) http_response_create((_code), CONTENT_TEXT, NULL, (_u8_vec))
#define HTTP_RES_EMPTY(_code) http_response_create((_code), CONTENT_TEXT, NULL, (u8_vec){0})
#define HTTP_RES_OK_JSON(_type, _instance) HTTP_RES_JSON(HTTP_OK, _type, (_instance)) 
#define HTTP_RES_OK_PLAIN(_u8_vec) HTTP_RES_PLAIN(HTTP_OK, (_u8_vec))
#define HTTP_RES_CSTR(_code, _cstr) HTTP_RES_PLAIN(_code, U8_VEC_STR(_cstr))
#define HTTP_RES_OK_CSTR(_cstr) HTTP_RES_CSTR(HTTP_OK, _cstr)

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
    HTTP_HEADERS_TOO_LARGE =  431,
    HTTP_INTERNAL_SERVER_ERROR = 500,
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

typedef struct __MCFW_HTTP_RESPONSE__ http_response;

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
    size_t content_length;
    u8_slice host;
    u8_slice accept;
};


typedef struct {
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

http_response* http_response_create(http_status_code c, http_content_type res_type, char* __type, u8_vec buff);

#endif // PUBLIC_HTTP_H
