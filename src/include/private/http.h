#ifndef PRIVATE_HTTP_H_
#define PRIVATE_HTTP_H_

#include <picohttpparser.h>
#include <public/http.h>
#include <public/router.h>
#include <private/version.h>

void req_destroy(HttpRequest* restrict req);
HttpStatusCode req_fill_core_headers(HttpRequest* restrict req, HttpContentType expected);
HttpStatusCode req_recv_headers(
    Arena* arena,
    int client_fd,
    HttpRequest* restrict req,
    _Out_ DStatic* restrict headers_buff,
    _Out_ DStatic* restrict unused_data
);
HttpStatusCode req_recv_body(
    int client_fd,
    HttpRequest* restrict req,
    AppContext* restrict app,
    DStatic crefl_type_name,
    const DStatic* restrict unused_data
) ;

typedef enum {
    ALIGN_HEADER,
    ALIGN_CRFL,
    ALIGN_CHUNK,
    ALIGN_COMPLETE,
    ALIGN_TRAILER
} StreamBuffAlignment;

typedef enum {
    MIME_FLAG_COMPRESS  = 1,
    MIME_FLAG_SERDE     = 1 << 1,
    MIME_FLAG_STATIC    = 1 << 2,
    MIME_FLAG_CACHE     = 1 << 3,
    MIME_FLAG_IS_TEXT   = 1 << 4
} MIMEFlags;

typedef enum {
    MIME_ALGO_NONE,
    MIME_ALGO_GZIP,
    MIME_ALGO_ZSTD,
    MIME_ALGO_BR,
    MIME_ALGO_DEFLATE
} MIMECompression;

typedef struct {
    const char* mime_str;
    const char* full_str;
    const char* ext;
    uint8_t flags;
    MIMECompression comp_algorithm;
} MIMELutEntry;

typedef struct {
    bool headers_sent;
    HttpTransmissionType encoding;
    StreamBuffAlignment alignment;
    uint8_t mime_flags;
    MIMECompression mime_compression;
    size_t total_bytes;
    int client_fd;

    DStatic trailing_data;
    DStatic resp_headers;
    union {
        struct {
            DBuffer buffer;
            size_t chunk_remaining;
        } chunked;

        DBuffer inm;

        // File data
        struct {
            int file_fd;
            time_t last_modified;
            DStatic range_header;
            DStatic file_name;
        } file;
    } data;
} HttpSender;

#define HTTP_RES_SIGNATURE 0x52455350 // "RESP"

struct HttpHeaderArray {
    struct HttpHeader* data;
    size_t count;
    size_t capacity;
};

typedef struct __MCFW_HTTP_RESPONSE__ {
    uint32_t signature;
    HttpStatusCode code;
    HttpContentType type;
    DStatic type_string;
    DStatic range_header;
    
    int minor_version;
    bool keep_alive;
    struct HttpHeaderArray response_headers;
    
    HttpSender stream;
} HttpResponse;



struct mime_ext_map { const char *name; int id; };
struct mime_str_map { const char *name; int id; };
const struct mime_str_map * mime_str_lookup(const char *str, size_t len);
const struct mime_ext_map * mime_ext_lookup (const char *str, size_t len);

void static_file_handler(
    const HttpRequest* req,
    HttpResponse* res,      
    DStatic* file_path,
    HttpContentType router_expect
);

void res_default(_Out_ HttpResponse* r, int client_fd);
void res_destroy(HttpResponse* res);
void res_send(HttpResponse* res, uint64_t timeout_ms);
void res_set_req_headers(HttpResponse* res, bool keep_alive, DStatic req_Range);

#define res_init(fd) ({\
    HttpResponse __res = {0};\
    res_default(&__res, fd);\
    __res;\
})



typedef enum {
    STREAMING_OK,
    STREAMING_TIMEOUT,
    STREAMING_OUT_OF_SPACE,
    STREAMING_IO_ERROR
} SendError;

void sender_default(_Out_ HttpSender* str, int client_fd);
void sender_destroy(HttpSender* str);

#define sender_init(_fd) ({         \
    Stream __strm = (Stream){0};    \
    sender_default(&__strm, (_fd)); \
    __strm;                         \
}) 

extern char g_cstr_buff[];

#define make_cstr(_ds) ({\
    DStatic __val = (_ds);\
    memcpy(g_cstr_buff, __val.s_ptr, __val.count);\
    g_cstr_buff[__val.count] = '\0';\
    g_cstr_buff;\
})

void sender_set_mime_flags(HttpSender* str, HttpContentType t);
void sender_configure_inm(HttpSender* str, DBuffer db, HttpContentType t);
void sender_configure_file(HttpSender* str, DStatic file_path, HttpContentType t, DStatic range_header);


_Async_ SendError sender_push(HttpSender* s, uint64_t timeout_ms);


#endif // PRIVATE_HTTP_H_