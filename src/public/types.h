#ifndef TYPES_H_
#define TYPES_H_

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <crefl/types.h>
#include <stdbool.h>


typedef struct {
    uint8_t* data;
    size_t count;
    size_t capacity;
} u8_vec;

typedef struct {
    uint8_t* data;
    size_t len;
} u8_slice;

#define U8_VEC_NULL (u8_vec){0}


#define U8_VEC(_buffer, _size) ({\
    void* __p = malloc(_size);\
    memcpy(__p, (_buffer), (_size));\
    (u8_vec){.data = (uint8_t*)__p, .count = _size, .capacity = _size };\
})

#define U8_VEC_NEW(_cap) ({\
    (u8_vec){.data = (uint8_t*)malloc(_cap), .count = 0, .capacity = _cap };\
})

#define U8_VEC_STR(_null_term_str) ({\
    size_t __s = strlen((_null_term_str));\
    U8_VEC((_null_term_str), __s);\
})

#define U8_VEC_FREE(_vec) free((_vec).data)

#define U8_SLICE(_buffer, _len) ({\
    (u8_slice){.data = (uint8_t*)(_buffer), .len = (_len)};\
})

#define U8_CSTR_SLICE(_null_term_str) ({\
    (u8_slice){.data = (uint8_t*)_null_term_str, .len = strlen(_null_term_str)};\
})

#define U8_CSTR_LIT_SLICE(_null_term_str) ({\
    (u8_slice){.data = (uint8_t*)_null_term_str, .len = sizeof(_null_term_str) - 1};\
})

#define U8_VIEW(_buffer, start, end) ({\
    U8_SLICE(_buffer + start, end - start);\
})

#define U8_VEC_FMT(vec) (int)((vec).count), (vec).data
#define U8_SLICE_FMT(slice) (int)(slice).len, (slice).data


typedef struct __MCFW__ROUTER__ router;



#endif // TYPES_H_