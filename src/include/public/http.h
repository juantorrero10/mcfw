#ifndef PUBLIC_HTTP_H
#define PUBLIC_HTTP_H

#include "buffers.h"

REFLECT(enum) {
    HTTP_CONTINUE = 100,
    HTTP_OK = 200,
    HTTP_PARTIAL_CONTENT = 206,
    HTTP_NOT_MODIFIED = 304,
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
} HttpStatusCode;

// Number of (valid) enum members.
#define CONTENT_TYPE_N_MEMB 16

typedef enum {
    // Value when the MIME media type isn't represented in this enum.
    // It will be available to lookup as string slice in the miscellaneous headers. 
    CONTENT_OTHER = -2,
    // When especified header is missing.
    CONTENT_NOT_SPECIFIED = -1,

    // text/...
    CONTENT_TEXT,
    CONTENT_HTML,
    CONTENT_JAVASCRIPT,
    CONTENT_EVENT_STREAM,
    CONTENT_CSS,
    CONTENT_JSON,
    CONTENT_WASM,
    CONTENT_XML,

    // application/..
    CONTENT_BINARY,
    CONTENT_XHTML,
    CONTENT_PDF,

    // image/...
    CONTENT_SVG,
    CONTENT_PNG,
    CONTENT_JPEG,
    CONTENT_ICO,

    
    // font
    CONTENT_WOFF2
} HttpContentType;


#define PAYLOAD_METHODS_IDX 3

REFLECT(enum) {
    GET,
    DELETE,
    HEAD,
    POST = 3,
    PUT,
    PATCH,
    OPTIONS
} HttpRequestMethod;

typedef enum {
    ENCODING_IDENTITY = 0, // No compression
    ENCODING_GZIP,
    ENCODING_DEFLATE,
    ENCODING_BR
} HttpCompression;

typedef enum {
    TRANSFER_NOT_SPECIFIED = -1,
    TRANSFER_INMEDIATE, // Content-Length: ----
    TRANSFER_CHUNKED,   // Transfer-Enconding: chunked
    TRANSFER_FILE       // Internal state
} HttpTransmissionType;

struct HttpHeader {
    DStatic header_name;
    DStatic val;
};

struct HttpHeaderHTable {
    struct HttpHeader* table;
    size_t n_headers;
    size_t capacity;
};

struct HttpCoreHeaders {
    bool conn_keep_alive;
    HttpContentType content_type;
    HttpTransmissionType tx_encoding;
    union {
        size_t content_length;
        size_t max_body_length;
    } payload_len;
    DStatic host;
    DStatic accept;
};


typedef struct __MCFW_HTTP_REQUEST__ {
    int minor_version;
    HttpRequestMethod r_method;
    DStatic path;

    bool was_deserialized;
    struct {
        DHeap body;
        struct {
            void* instance;
            RStruct* type;
        } deserialization;
    } content;

    // Essential Headers easy access 
    struct HttpCoreHeaders core_headers;
    /**
     * @brief Hashtable with all the headers.
     * use ```req_get_header_value()``` to lookup a header
     */ 
    struct HttpHeaderHTable header_table;
} HttpRequest;

typedef struct __MCFW_HTTP_RESPONSE__ HttpResponse;


/**
 * @brief Try to find the value of a header inside the http request
 * @note Search is O(1) because is a hash table. 
 * @returns String slice of the value or an invalid DStatic if not present. 
 */
DStatic req_get_header_value(const HttpRequest* req, const DStatic name);

/**
 * @brief wrapper to req->body.deserialized
 * @returns NULL if no deserialization happened. 
 * This probably means the route was badly configured.
 * @note to obtain the instance just cast it 
 * (ex: ```Type* instance = req_get_deserialized_body(req)```)
 * @attention Do not try to free the data
 */
void* req_get_deserialized_body(const HttpRequest* req);

/**
 * @note this is the default state of the content
 */
void res_set_content_empty(HttpResponse* res);

/**
 * @note It is recommended to use the macro ```res_set_content``` 
 * for automatic buffer type generalization 
 */
void __res_set_content(HttpResponse* res, HttpContentType t, const DBuffer buff);

/**
 * @brief Set a buffer as the the body of the response.
 * @note This is macro wrapper around ```__res_set_content``` 
 * to provide buffer type generalization.
 * @param resp_ptr HttpResponse*
 * @param content_type HttpContentType
 * @param buff DStatic, DHeap or DBuffer, it will be generalized to DBuffer
 * @attention Ownership of buff gets transfered to the framework, 
 * this means that it be freed (if necessary) automatically after sending the response.   
 */
#define res_set_content(resp_ptr, content_type, buff) \
    __res_set_content(resp_ptr, content_type, DB_FROM(buff));


/**
 * @note It is recommended to use the macro ```res_set_content_serialize``` to not misspell the type
 */
void __res_set_content_serialize__(HttpResponse* res, HttpContentType t, const char* ctype, const void* instance);

/**
 * @brief Serialize an instance and set it as the content of the response.
 * @param res HttpResponse*
 * @param content_type HttpContentType
 * @param reflected_type name of the typedef struct of the reflected type
 * @param instance void* pointer to var 
 * @note This is macro wrapper around ```__res_set_content_serialize__``` 
 * to stringify the type name.
 */
#define res_set_content_serialize(res, content_type, reflected_type, instance) \
    __res_set_content_serialize__(res, content_type, #reflected_type, instance)

/**
 * @brief Send the contents of a file as a response
 * @param res HttpResponse*
 * @param t if CONTENT_NOT_SPECIFIED it will deduce it thought the file externsion
 * @param downloadable Make the target browser download as a file through the Content-Disposition header
 */
void res_set_content_file(HttpResponse* res, HttpContentType t, const DStatic file_path, bool downloadable);



void res_set_content_type(HttpResponse* res, const DStatic MIME_type_name);

/**
 * @brief Set HTTP status code that will be sent to the client.
 * @note HTTP_OK (200) is the default code, if this code is desired
 * you can omit calling this function
 */
void res_set_status_code(HttpResponse* res, HttpStatusCode code);

/**
 * @brief Add a header to the HTTP response.
 * @returns true if headers was successfully added. 
 * There is a limit of 128 headers, after which, false is returned
 */
bool res_add_header(HttpResponse* res, const DStatic name, const DStatic value);

/**
 * @brief Include "Connection: Close" in the headers and close the client socket.
 */
void res_close_connection(HttpResponse* res);


#endif // PUBLIC_HTTP_H
