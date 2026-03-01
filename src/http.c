#define _GNU_SOURCE

#include <stdlib.h>

#include <crefl/core.h>

#include "public/http_interface.h"
#include "private/types.h"
#include "private/log.h"
#include "private/glob.h"

// Feels overkill but in the future way may have more formats
const int req_deserializing_lut[3] = {
    [CONTENT_TEXT] = 0, [CONTENT_HTML] = 0, [CONTENT_JSON] = 1,
};

const int req_serializing_lut[3] = {
    [CONTENT_HTML] = 0,
    [CONTENT_JSON] = 1,
    [CONTENT_TEXT] = 0,
};


char* str_response_type_lut[3] = {
    [CONTENT_HTML] = "text/html",
    [CONTENT_JSON] = "application/json",
    [CONTENT_TEXT] = "text/plain"
};


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

http_request http_req_create(http_content_type t, u8_slice body) { 
    return (http_request){.type = t, .body = body};
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


#define ROUT_RET_ERR(_err) (routing_result){.data.err = (_err), .st = MCFW_ERR};
#define ROUT_RET_OK(_fptr) (routing_result){.data.ok_data = (_fptr), .st = MCFW_OK};

#define REQUEST_METHOD_MAX 7
#define REQUEST_METHOD_MIN 3

#define MAX(a, b) ((a) > (b))? (a) : (b)


/**
 * @brief mock route protocol <METHOD> <ROUTE>\r\n\r\n[<body>] -> GET /about\r\n\r\n, POST /getuser\r\n\r\n{"id":2}
 */
routing_result http_mock_route_parser(u8_vec* full_buffer, http_request* out_buffer) {
    size_t consumed = 0;
    // Get method (or request kind)
    // First -> mandatory space between method and route
    char* tok_end = memchr(full_buffer->data, ' ', full_buffer->count);
    if (!tok_end) return ROUT_RET_ERR(HTTP_BAD_REQUEST);

    size_t curr_tok_size = (uint8_t*)tok_end - full_buffer->data;
    // Bounds check first.
    if (curr_tok_size > REQUEST_METHOD_MAX || curr_tok_size < REQUEST_METHOD_MIN) return ROUT_RET_ERR(HTTP_METHOD_NOT_ALLOWED);
    
    u8_slice method = U8_VIEW(full_buffer->data, 0, curr_tok_size);
    consumed += curr_tok_size + 1;
    
    request_kind r = _get_req_from_str(&method);
    if ((int)r == (int)-1) return ROUT_RET_ERR(HTTP_METHOD_NOT_ALLOWED);

    // Get the route (path)
    // Second -> mandatory space between route and http version
    tok_end = memchr((char*)full_buffer->data + curr_tok_size + 1, ' ', MAX(full_buffer->count - (curr_tok_size + 1), 0));
    if (!tok_end) return ROUT_RET_ERR(HTTP_BAD_REQUEST);
    curr_tok_size = (size_t)tok_end - ((size_t)method.data + method.len + 1); 

    u8_slice path = U8_VIEW(method.data + method.len + 1, 0, curr_tok_size);
    consumed += curr_tok_size + 1;

    // Skip until the end of headers and get the body.
    tok_end =  memmem(full_buffer->data + consumed, full_buffer->count - consumed, "\r\n\r\n", 4);
    size_t content_length = 0;
    u8_slice body = {0};
    if (tok_end) {
        // Does have a body
        content_length = MAX(full_buffer->count - (size_t)(tok_end - (char*)full_buffer->data) - 4, 0); 
        body = U8_VIEW(tok_end + 4, 0, content_length);
    }
    *out_buffer = (http_request){
        .body = body,
        .r_kind = r,
        .path = path,
        .content_length = content_length,
        // Default values
        .http_version = 1,
        .decoded = {0},
        .type = CONTENT_TEXT,
    };
    return ROUT_RET_OK(NULL);
}