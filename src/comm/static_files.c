#define _GNU_SOURCE

#include "../public/http.h"
#include "../private/http.h"
#include "../private/comm.h"

mime_lut_entry MIME_LUT[CONTENT_TYPE_N_MEMB];

// get content type by extension
struct mime_ext_map { const char *name; int id; };
const struct mime_ext_map *
mime_ext_lookup (const char *str, size_t len);

static inline u8_slice get_extension(u8_slice* file_name) {
    uint8_t* ptr = (uint8_t*)memrchr(file_name->data, '.', file_name->len);
    if (!ptr) return (u8_slice){0};
    return SLICE_FROM(ptr, file_name->len - (ptr - file_name->data));
}


http_response* static_file_handler(http_request* req, u8_slice* file_name, http_content_type router_expect) {
    // TEMP
    http_content_type t = router_expect;
    if (t == CONTENT_NOT_SPECIFIED) {
        u8_slice ext = get_extension(file_name);
        const struct mime_ext_map * _res = NULL;
        if (!ext.data) t = CONTENT_BINARY;
        else {
            _res = mime_ext_lookup((const char*)ext.data, ext.len);
            t = (_res)? (http_content_type)_res->id : CONTENT_BINARY;
        }
    }

    http_response* r =  http_response_create(HTTP_OK, t, SLICE_CSTR_LIT(MIME_LUT[t].full_str), NULL, VEC_FROM_SLICE((*file_name)), TRANSFER_FILE);
    u8_slice fn = *file_name;
    stream_file(&r->stream, -1, fn, req);
    return r;
}



__attribute__((section("rdata")))
mime_lut_entry MIME_LUT[CONTENT_TYPE_N_MEMB] = {
    [CONTENT_TEXT] = {
        .mime_str = "text/plain",
        .full_str = "text/plain; charset=utf-8",
        .ext = ".txt",
        .flags = MIME_FLAG_COMPRESS | MIME_FLAG_STATIC | MIME_FLAG_IS_TEXT,
        .comp_algorithm = MIME_ALGO_BR
    },
    [CONTENT_HTML] = {
        .mime_str = "text/html",
        .full_str = "text/html; charset=utf-8",
        .ext = ".html",
        .flags = MIME_FLAG_COMPRESS | MIME_FLAG_STATIC | MIME_FLAG_CACHE | MIME_FLAG_IS_TEXT,
        .comp_algorithm = MIME_ALGO_BR
    },[CONTENT_JAVASCRIPT] = {
        .mime_str = "text/javascript",
        .full_str = "text/javascript; charset=utf-8",
        .ext = ".js",
        .flags = MIME_FLAG_COMPRESS | MIME_FLAG_STATIC | MIME_FLAG_CACHE | MIME_FLAG_IS_TEXT,
        .comp_algorithm = MIME_ALGO_BR
    },[CONTENT_EVENT_STREAM] = {
        .mime_str = "text/event-stream",
        .full_str = "text/event-stream; charset=utf-8",
        .ext = NULL,
        .flags = MIME_FLAG_COMPRESS | MIME_FLAG_STATIC | MIME_FLAG_CACHE | MIME_FLAG_IS_TEXT,
        .comp_algorithm = MIME_ALGO_BR
    },[CONTENT_CSS] = {
        .mime_str = "text/css",
        .full_str = "text/css; charset=utf-8",
        .flags = MIME_FLAG_COMPRESS | MIME_FLAG_STATIC | MIME_FLAG_CACHE | MIME_FLAG_IS_TEXT,
        .comp_algorithm = MIME_ALGO_BR
    },[CONTENT_JSON] = {
        .mime_str = "application/json",
        .full_str = "application/json; charset=utf-8",
        .ext = ".json",
        .flags = MIME_FLAG_COMPRESS | MIME_FLAG_SERDE | MIME_FLAG_CACHE | MIME_FLAG_IS_TEXT,
        .comp_algorithm = MIME_ALGO_ZSTD
    },[CONTENT_XML] = {
        .mime_str = "application/xml",
        .full_str = "application/xml; charset=utf-8",
        .ext = NULL,
        .flags = MIME_FLAG_COMPRESS | MIME_FLAG_STATIC | MIME_FLAG_CACHE | MIME_FLAG_IS_TEXT,
        .comp_algorithm = MIME_ALGO_ZSTD
    },[CONTENT_WASM] = {
        .mime_str = "application/wasm",
        .full_str = "application/wasm",
        .ext = ".wasm",
        .flags = MIME_FLAG_STATIC | MIME_FLAG_CACHE,
        .comp_algorithm = MIME_ALGO_NONE
    },[CONTENT_BINARY] = {
        .mime_str = "application/octet-stream",
        .full_str = "application/octet-stream",
        .ext = ".bin",
        .flags = MIME_FLAG_COMPRESS | MIME_FLAG_STATIC,
        .comp_algorithm = MIME_ALGO_BR
    },[CONTENT_XHTML] = {
        .mime_str = "application/xhtml+xml",
        .full_str = "application/xhtml+xml; charset=utf-8",
        .ext = ".xhtml",
        .flags = MIME_FLAG_COMPRESS | MIME_FLAG_STATIC | MIME_FLAG_CACHE | MIME_FLAG_IS_TEXT,
        .comp_algorithm = MIME_ALGO_BR
    },[CONTENT_PDF] = {
        .mime_str = "application/pdf",
        .full_str = "application/pdf",
        .ext = ".pdf",
        .flags = MIME_FLAG_STATIC | MIME_FLAG_CACHE,
        .comp_algorithm = MIME_ALGO_NONE
    },[CONTENT_SVG] = {
        .mime_str = "image/svg+xml",
        .full_str = "image/svg+xml",
        .ext = ".svg",
        .flags = MIME_FLAG_COMPRESS | MIME_FLAG_STATIC | MIME_FLAG_CACHE,
        .comp_algorithm = MIME_ALGO_ZSTD
    },[CONTENT_PNG] = {
        .mime_str = "image/png",
        .full_str = "image/png",
        .ext = ".png",
        .flags = MIME_FLAG_STATIC | MIME_FLAG_CACHE,
        .comp_algorithm = MIME_ALGO_NONE
    },[CONTENT_JPEG] = {
        .mime_str = "image/jpeg",
        .full_str = "image/jpeg",
        .ext = ".jpeg",
        .flags = MIME_FLAG_STATIC | MIME_FLAG_CACHE,
        .comp_algorithm = MIME_ALGO_NONE
    },[CONTENT_ICO] = {
        .mime_str = "image/vnd.microsoft.icon",
        .full_str = "image/vnd.microsoft.icon",
        .ext = ".ico",
        .flags = MIME_FLAG_COMPRESS | MIME_FLAG_STATIC | MIME_FLAG_CACHE,
        .comp_algorithm = MIME_ALGO_BR
    },[CONTENT_WOFF2] = {
        .mime_str = "font/woff2",
        .full_str = "font/woff2",
        .ext = ".woff2",
        .flags = MIME_FLAG_STATIC | MIME_FLAG_COMPRESS,
        .comp_algorithm = MIME_ALGO_NONE,
    }
};
