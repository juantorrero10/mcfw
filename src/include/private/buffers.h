#ifndef PRIVATE_BUFFERS_H_
#define PRIVATE_BUFFERS_H_


#include "../public/buffers.h"

#ifdef BUFFERS_IMPLEMENTATION
void dheap_concat(DHeap* v, const DStatic* s) {
    if (v->count + s->count > v->capacity) {
        DH_GROW(v);
    }
    //SIGSEGV
    memcpy(v->h_ptr + v->count, s->s_ptr, s->count);
    v->count += s->count;
}
bool dstatic_concat(DStatic* v, const DStatic* s) {
    if (v->count + s->count > v->capacity) {
        return false;
    }
    memcpy(v->s_ptr + v->count, s->s_ptr, s->count);
    v->count += s->count;
    return true;   
}
#endif

#endif // PRIVATE_BUFFERS_H_