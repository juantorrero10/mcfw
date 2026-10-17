#define _GNU_SOURCE

#include <private/mcfw.h>

MIMELutEntry MIME_LUT[CONTENT_TYPE_N_MEMB];


static inline DStatic get_extension(DStatic* file_name) {
    uint8_t* ptr = (uint8_t*)memrchr(file_name->s_ptr, '.', file_name->count);
    if (!ptr) return (DStatic){0};
    size_t len = (size_t)file_name->s_ptr - ((size_t)ptr - file_name->count);
    return DS_FROM(ptr, len, len);
}

void static_file_handler(
    const HttpRequest* req,
    HttpResponse* res,      
    DStatic* file_path,
    HttpContentType router_expect
) {
    // TEMP
    (void)req;
    HttpContentType t = router_expect;
    if (t == CONTENT_NOT_SPECIFIED) {
        DStatic ext = get_extension(file_path);
        const struct mime_ext_map * _res = NULL;
        if (!ext.s_ptr) t = CONTENT_BINARY;
        else {
            _res = mime_ext_lookup((const char*)ext.s_ptr, ext.count);
            t = (_res)? (HttpContentType)_res->id : CONTENT_BINARY;
        }
    }

    res->type_string = DS_CSTR((char*)MIME_LUT[t].full_str);
    res_set_content_file(res, t, *file_path, false);
}



__attribute__((section("rdata")))
MIMELutEntry MIME_LUT[CONTENT_TYPE_N_MEMB] = {
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
