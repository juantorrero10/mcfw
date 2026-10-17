#ifndef ROUTER_PRIV_H_
#define ROUTER_PRIV_H_

#include <public/router.h>
#include <public/http.h>


#define ROUTER_SIGNATURE 0x524F5452 // "ROTR"

typedef struct __MCFW__ROUTER__ {
    DHeap routes_str_pool;
    DHeap types_str_pool;
    uint32_t signature;
    DYN_ARRAY(RouterEntry) entries;
} Router;


typedef struct {
    bool is_ok;
    union {
        void* ok_data;
        HttpStatusCode err;
    } data;
} RoutingResult;



typedef Router* (*AppEntryFn)(void);
 
void router_destroy(Router* r);
RoutingResult router_get_entry(Router* r, HttpRequest* req);

#endif // ROUTER_PRIV_H_