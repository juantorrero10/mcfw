#include <stdlib.h>

#include "public/response.h"
#include "private/types.h"
#include "private/log.h"

http_response* http_response_create(http_status_code c, http_content_type res_type, char* __type, u8_vec buff) {
    http_response* r = malloc(sizeof(http_response));
    (void)__type;
    r->signature = HTTP_RES_SIGNATURE;
    if (res_type != CONTENT_TEXT) { 
        mlog_warn("http_response_create", "only text supported.");
        r->buff = U8_VEC_NULL;
        r->code = HTTP_OK;
        r->type = CONTENT_TEXT;
        return r;
    }
    r->buff = buff;
    if (buff.count > 0) {
        r->buff.data = malloc(buff.count);
        memcpy(r->buff.data, buff.data, buff.count);
    }
    r->code = c;
    r->type = res_type;
    return r;
}