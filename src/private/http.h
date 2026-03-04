#ifndef PRIVATE_HTTP_H_
#define PRIVATE_HTTP_H_

#include "../vendor/picohttpparser.h"
#include "../public/types.h"
#include "../public/router.h"


#define HTTP_RES_SIGNATURE 0x52455350 // "RESP"

typedef struct __MCFW_HTTP_RESPONSE__ {
    uint32_t signature;
    http_status_code code;

    http_content_type type;
    u8_slice type_string;
    
    uint64_t flags;
    
    bool serialize;
    char* serialize_typename;

    u8_vec buff;
} http_response;

void http_create_misc_headers_hashtable(struct phr_header* headers, struct http_header* out_headers, size_t buff_size);
http_status_code http_check_core_headers(http_request* restrict req, router_entry* restrict re);

#endif // PRIVATE_HTTP_H_