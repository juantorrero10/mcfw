#ifndef ROUTER_H_
#define ROUTER_H_

#include "types.h"

router* router_create(router_entry routes[], size_t amount);

#define router_new(entry_arr) router_create(entry_arr, sizeof(entry_arr) / sizeof(router_entry))

#endif // ROUTER_H_