#ifndef PRIVATE_TYPES_H_
#define PRIVATE_TYPES_H_

#include <crefl/types.h>
#include "../public/types.h"

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

#define ROUTER_SIGNATURE 0x524F5452 // "ROTR"

typedef struct __MCFW__ROUTER__ {
    u8_vec routes_str_pool;
    u8_vec types_str_pool;
    uint32_t signature;
    DYN_ARRAY(router_entry) entries;
} router;

#define HTTP_RES_SIGNATURE 0x52455350 // "RESP"

typedef struct __MCFW_HTTP_RESPONSE__ {
    uint32_t signature;
    http_status_code code;
    http_content_type type;
    char* typename;
    u8_vec buff;
} http_response;

typedef router* (*app_entry_fn)(void);

typedef struct {
    void* c_handle;
    size_t c_ctx_ref_count; // N of ctxes using this version.
    size_t at_index;        // Position on the array of pointers
    uint16_t c_checksum[16];
} crefl_scope;


typedef struct {
    uint32_t reload_version;
    uint32_t ref_count;         // N of clients using the this router.
    void* dl_handle;
    size_t at_index;        // Position on the array of pointers
    crefl_scope* crefl_data;
    router router;
} app_context;

typedef struct {
    // Occupation map to keep the contexes packed 
    uint8_t* occupied;
    DYN_ARRAY(app_context*) t;
} app_context_table;

typedef struct {
    uint8_t* occupied;
    DYN_ARRAY(crefl_scope*) t;
} crefl_scope_table;

#endif // PRIVATE_TYPES_H_