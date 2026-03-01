#ifndef PRIVATE_HTTP_H_
#define PRIVATE_HTTP_H_

#include "types.h"
/**
 * @brief mock route protocol <METHOD> <ROUTE>\r\n\r\n[<body>] -> GET /about\r\n\r\n, POST /getuser\r\n\r\n{"id":2}
 */
routing_result http_mock_route_parser(u8_vec* full_buffer, http_request* out_buffer);

#endif // PRIVATE_HTTP_H_