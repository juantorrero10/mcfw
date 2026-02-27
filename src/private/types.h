#ifndef PRIVATE_TYPES_H_
#define PRIVATE_TYPES_H_

#include <crefl/types.h>
#include "../public/types.h"

typedef void* (*async_fn)(void*);

typedef enum {
    MCFW_OK,
    MCFW_ERR,
} mcfw_status;

typedef struct {
    mcfw_status st;
    union {
        async_fn method_fptr;
        http_status_code err;
    } data;
} routing_result;

#define ROUTER_SIGNATURE 0x524F5452 // "ROTR"

typedef struct __MCFW__ROUTER__ {
    u8_vec route_str_pool;
    uint32_t signature;
    DYN_ARRAY(router_entry) entries;
} router;

#define HTTP_RES_SIGNATURE 0x52455350 // "RESP"

typedef struct __MCFW_HTTP_RESPONSE__ {
    uint32_t signature;
    http_status_code code;
    http_content_type type;
    u8_vec buff;
} http_response;

#endif // PRIVATE_TYPES_H_