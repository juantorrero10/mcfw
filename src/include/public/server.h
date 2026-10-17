#ifndef PUBLIC_SERVER_H_
#define PUBLIC_SERVER_H_


typedef void* RHandle;

void server_stop();
uint64_t server_get_total_clients();
RHandle server_get_crefl_handle();


#endif // PUBLIC_SERVER_H_