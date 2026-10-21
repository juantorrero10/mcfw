#ifndef TYPES_H_
#define TYPES_H_

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <crefl/types.h>
#include <stdbool.h>
#include <stdlib.h>

/**
 * @brief This parameter is optional and will be explicitly checked if its NULL.
 * @note In absense of this marker, argument will probably triggrer as SIGSEGV if passed NULL.
 */
#define _Opt_
/**
 * @brief This parameter is passed as a pointer to be written to by callee.
 */
#define _Out_


typedef struct {
    void* h_ptr;
    size_t count;
    size_t capacity;
} DHeap;

typedef struct {
    void* s_ptr;
    size_t count;
    size_t capacity;
} DStatic;


// Generalization of DHeap and DStatic
typedef struct {
    bool is_heap_alloced;
    union {
        DHeap hp;
        DStatic st;
    } buff;
} DBuffer;



#define DH_EMPTY() (DHeap){0}


#define DH_FROM_RAW(_buffer, _size) ({\
    void* __p = malloc(_size);\
    memcpy(__p, (_buffer), (_size));\
    (DHeap){.h_ptr = (uint8_t*)__p, .count = _size, .capacity = _size };\
})

#define DH_FROM(_var_ptr) ({\
    void* __p = (_var_ptr);\
    size_t __s = sizeof(*(_var_ptr));\
    DH_FROM_RAW(__p, __s);\
})



#define DH_FROM_SLICE(_slice) ({\
    DH_FROM_RAW((_slice).s_ptr, (_slice).count);\
})

#define DH_NEW(_cap) ({\
    (DHeap){.h_ptr = (uint8_t*)malloc(_cap), .count = 0, .capacity = _cap };\
})

#define DH_CSTR(_null_term_str) ({\
    size_t __s = strlen((_null_term_str));\
    DH_FROM_RAW((_null_term_str), __s);\
})


#define DH_LIT(_str_lit) ({\
    size_t __s = sizeof(_str_lit) - 1;\
    DH_FROM_RAW((_str_lit), __s);\
})

#define DH_GROW(_dheap_ptr) do {\
    DHeap* __dh = (_dheap_ptr);\
    __dh->capacity <<= 1;\
    __dh->h_ptr = realloc(__dh->h_ptr, __dh->capacity);\
} while(0)

#define DH_FREE(_vec) free((_vec).h_ptr)

#define DS_IS_VALID(_slice) ((_slice).s_ptr != NULL)
#define DH_IS_VALID(_dheap) ((_dheap).h_ptr != NULL)

#define DS_EMPTY() (DStatic){0}

#define DS_FROM(_buffer, _count, _cap) ({\
    (DStatic){.s_ptr = (_buffer), .count = (_count), .capacity = (_cap)};\
})

#define DS_FROM_DH(_ds) *(DStatic*)&(_ds)

#define DS_CSTR(_null_term_str) ({\
    size_t __s = strlen(_null_term_str);\
    DS_FROM(_null_term_str, __s, __s);\
})

#define DS_LIT(_null_term_str) ({\
    size_t __s = sizeof(_null_term_str) - 1;\
    DS_FROM(_null_term_str, __s, __s);\
})

#define DS_VIEW(_buffer, start, end) ({\
    size_t __s = end - start;\
    DS_FROM(_buffer + start, __s, __s);\
})

#define DH_WRITE(_dheap, fmt, ...) do {\
    (_dheap).count += snprintf((char*)((_dheap).h_ptr + (_dheap).count), (_dheap).capacity, fmt, __VA_ARGS__);\
} while (0)

#define DS_WRITE(_dstatic, fmt, ...) do {\
    (_dstatic).count += snprintf((char*)((_dstatic).s_ptr + (_dstatic).count), (_dstatic).capacity, fmt, __VA_ARGS__);\
} while (0)

#define DH_ARGS(dheap) (int)((dheap).count), (char*)(dheap).h_ptr
#define DS_ARGS(dstatic) (int)((dstatic).count), (char*)(dstatic).s_ptr

// Conversion from DStatic or DHeap into DBuffer (pass by value, otherwise it wont work)
#define DB_FROM(__buff) ({\
    DBuffer __res; \
    __typeof__(__buff) __val = (__buff);\
    _Generic(__val, \
        DHeap:   (__res.is_heap_alloced = true, __res.buff.hp = *(DHeap*)&(__val)), \
        DStatic: (__res.is_heap_alloced = false, __res.buff.st = *(DStatic*)&(__val)), \
        DBuffer: (__res = *(DBuffer*)&(__val)) \
    );\
    __res;\
})

void dheap_concat(DHeap* v, const DStatic* s);
bool dstatic_concat(DStatic* v, const DStatic* s);



#endif // TYPES_H_