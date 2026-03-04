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

#define VEC_NULL (u8_vec){0}


#define VEC_COPY(_buffer, _size) ({\
    void* __p = malloc(_size);\
    memcpy(__p, (_buffer), (_size));\
    (u8_vec){.data = (uint8_t*)__p, .count = _size, .capacity = _size };\
})

#define VEC_FROM(_buffer, _size, _capacity) ({\
    (u8_vec){.data = (uint8_t*)_buffer, .count = _size, .capacity = _capacity };\
})

#define VEC_NEW(_cap) ({\
    (u8_vec){.data = (uint8_t*)malloc(_cap), .count = 0, .capacity = _cap };\
})

#define VEC_CSTR(_null_term_str) ({\
    size_t __s = strlen((_null_term_str));\
    VEC_FROM((_null_term_str), __s, __s);\
})


#define VEC_CSTR_LIT(_str_lit) ({\
    size_t __s = sizeof(_str_lit) - 1;\
    VEC_FROM((_str_lit), __s, __s);\
})

#define VEC_FREE(_vec) free((_vec).data)

#define SLICE_FROM(_buffer, _len) ({\
    (u8_slice){.data = (uint8_t*)(_buffer), .len = (_len)};\
})

#define SLICE_CSTR(_null_term_str) ({\
    (u8_slice){.data = (uint8_t*)_null_term_str, .len = strlen(_null_term_str)};\
})

#define SLICE_CSTR_LIT(_null_term_str) ({\
    (u8_slice){.data = (uint8_t*)_null_term_str, .len = sizeof(_null_term_str) - 1};\
})

#define SLICE_VIEW(_buffer, start, end) ({\
    SLICE_FROM(_buffer + start, end - start);\
})

#define VEC_FMT(vec) (int)((vec).count), (vec).data
#define SLICE_FMT(slice) (int)(slice).len, (slice).data


typedef struct __MCFW__ROUTER__ router;



#endif // TYPES_H_