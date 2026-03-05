#ifndef PRIVATE_TYPES_H_
#define PRIVATE_TYPES_H_

#include <crefl/types.h>
#include "../public/types.h"
#include "../public/router.h"

typedef void* (*async_fn)(void*);

typedef enum {
    MCFW_OK,
    MCFW_ERR,
} mcfw_status;

typedef struct {
    mcfw_status st;
    union {
        void* ok_data;
        http_status_code err;
    } data;
} routing_result;


typedef struct {
    mcfw_status st;
    union {
        http_status_code error_code;
        u8_vec* vec;
    } data;
} streaming_result;



#endif // PRIVATE_TYPES_H_