#ifndef PUBLIC_RESPONSE_H_
#define PUBLIC_RESPONSE_H_

#include "types.h"

#define HTTP_RES_JSON(_code, _type, _instance)  http_response_create(_code, APP_JSON, #_type, (u8_vec){.data = (uint8_t*)_instance, .count = sizeof(_type), .capacity = sizeof(_type)})
#define HTTP_RES_PLAIN(_code, _buffer, _size) http_response_create(_code, APP_TEXT, NULL, (u8_vec){.data = (uint8_t*)_buffer, .capacity = _size, .count = _size})
#define HTTP_RES_EMPTY(_code) http_response_create(_code, APP_TEXT, NULL, (u8_vec){0})
#define HTTP_RES_OK_JSON(_type, _instance) HTTP_RES_JSON(HTTP_OK, _type, _instance) 
#define HTTP_RES_OK_PLAIN(_buffer, _size) HTTP_RES_PLAIN(HTTP_OK, _buffer, _size)

http_response* http_response_create(http_status_code c, http_content_type res_type, char* __type, u8_vec buff);

#endif // PUBLIC_RESPONSE_H_
