#include <stdlib.h>

#include "public/http_interface.h"
#include "private/types.h"
#include "private/log.h"

http_response* http_response_create(http_status_code c, http_content_type res_type, char* __type, u8_vec buff) {
    http_response* r = malloc(sizeof(http_response));
    *r = (http_response){0};
    (void)__type;
    r->signature = HTTP_RES_SIGNATURE;
    if (res_type != CONTENT_TEXT) { 
        mlog_warn("only text supported.");
        r->buff = U8_VEC_NULL;
        r->code = HTTP_OK;
        r->type = CONTENT_TEXT;
        return r;
    }
    r->buff = buff;
    r->code = c;
    r->type = res_type;
    return r;
}

http_request http_req_create(http_content_type t, u8_slice body) { 
    return (http_request){.type = t, .buff = body};
}