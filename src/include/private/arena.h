#ifndef PRIVATE_ARENA_H_
#define PRIVATE_ARENA_H_

#include <private/buffers.h>

typedef struct {
    void* p;
    size_t used;
    size_t alloc_size;
} Arena;

Arena mcfw_arena_init(size_t c);
void mcfw_arena_reset(Arena* a);
DStatic mcfw_arena_alloc(Arena* a, size_t sz, bool align);
void mcfw_arena_dealloc(Arena* a, size_t sz);
void mcfw_arena_destroy(Arena* a);

#ifdef ARENA_IMPLEMENTATION
Arena mcfw_arena_init(size_t c) {
    return (Arena){
        .p = malloc(c),
        .used = 0,
        .alloc_size = c
    };
}
void mcfw_arena_reset(Arena* a) { a->used = 0; }
#define ALIGN 3
#define ALIGN_TO(var, align) var += ((1 << (align)) - ((var) & ((1 << (align)) - 1))) & ((1 << align) - 1);
DStatic mcfw_arena_alloc(Arena* a, size_t sz, bool align) { 
    if (align) {
        ALIGN_TO(a->used, ALIGN);
        ALIGN_TO(sz, ALIGN);
    }
    size_t new_used = a->used + sz;
    assert(new_used <= a->alloc_size);
    DStatic ret = DS_FROM(a->p + a->used, 0, new_used - a->used);
    a->used = new_used;
    return ret;
}
#undef ALING
#undef ALIGN_TO
void mcfw_arena_dealloc(Arena* a, size_t sz) { a->used = (sz > a->used)? 0 : a->used - sz; }
void mcfw_arena_destroy(Arena* a) { free(a->p); }
#endif


#endif // PRIVATE_ARENA_H_