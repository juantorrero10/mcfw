#ifndef PUBLIC_SERVER_H_
#define PUBLIC_SERVER_H_

#include <asclib/sync.h>

oneshot_tx_ref_t server_get_graceful_shutdown_tx();

#endif // PUBLIC_SERVER_H_