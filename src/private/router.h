#ifndef ROUTER_PRIV_H_
#define ROUTER_PRIV_H_

#include "../public/types.h"
#include "types.h"
 
void router_destroy(router* r);
routing_result router_get_handler(router* r, u8_slice method, u8_slice route);

#endif // ROUTER_PRIV_H_