#ifndef ROUTER_PRIV_H_
#define ROUTER_PRIV_H_

#include "../public/types.h"
#include "types.h"

 
void mcfw_register_router(router* r);
routing_result mcfw_get_handler(u8_vec* method, u8_vec* route, uint32_t version);

#endif // ROUTER_PRIV_H_