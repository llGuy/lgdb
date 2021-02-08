#include <stdlib.h>
#include "lgdb_alloc.h"


lgdb_linear_allocator_t lgdb_create_linear_allocator(uint32_t msize) {
    void *mem = malloc(msize);

    lgdb_linear_allocator_t allocator = {
        .max_size = msize,
        .start = mem,
        .current = mem
    };

    return allocator;
}


void *lgdb_lnmalloc(lgdb_linear_allocator_t *allocator, uint32_t size) {
    void *p = allocator->current;
    allocator->current = (void *)((uint8_t *)(allocator->current)+size);
    return p;
}


void lgdb_lnclear(lgdb_linear_allocator_t *allocator) {
    allocator->current = allocator->start;
}
