#ifndef PUBLIC_SERVER_H_
#define PUBLIC_SERVER_H_

#include <asclib/sync.h>

oneshot_tx_ref_t resources_graceful_shutdown();
uint64_t* resources_total_clients();


#endif // PUBLIC_SERVER_H_