#ifndef ROUTER_PRIV_H_
#define ROUTER_PRIV_H_

#include "../public/router.h"


#define ROUTER_SIGNATURE 0x524F5452 // "ROTR"

typedef struct __MCFW__ROUTER__ {
    u8_vec routes_str_pool;
    u8_vec types_str_pool;
    uint32_t signature;
    DYN_ARRAY(router_entry) entries;
} router;

#include "types.h"

typedef router* (*app_entry_fn)(void);
 
void router_destroy(router* r);
routing_result router_get_entry(router* r, http_request* req);

#endif // ROUTER_PRIV_H_