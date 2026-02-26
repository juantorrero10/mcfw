#ifndef ROUTER_PRIV_H_
#define ROUTER_PRIV_H_

#include "../public/types.h"
#include "types.h"
 
void router_destroy(router* r);
routing_result router_get_handler(router* r, u8_vec* method, u8_vec* route);

#endif // ROUTER_PRIV_H_