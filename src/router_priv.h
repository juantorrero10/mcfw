#ifndef ROUTER_PRIV_H_
#define ROUTER_PRIV_H_

#include "public/types.h"
 
void mcfw_register_router(router* r);
routing_result mcfw_handler(char* method, char* route, uint32_t version);

#endif // ROUTER_PRIV_H_