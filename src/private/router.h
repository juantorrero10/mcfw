#ifndef ROUTER_PRIV_H_
#define ROUTER_PRIV_H_

#include "../public/types.h"
#include "types.h"
 
void router_destroy(router* r);
routing_result router_get_entry(router* r, http_request* req);

#endif // ROUTER_PRIV_H_