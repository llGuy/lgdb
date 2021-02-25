#include <stdio.h>
#include <stdlib.h>
#include "lgdb_alloc.h"
#include "lgdb_utility.h"
#include <Windows.h>


lgdb_linear_allocator_t lgdb_create_linear_allocator(uint32_t msize) {
    void *mem = malloc(msize);

    lgdb_linear_allocator_t allocator = {
        .max_size = msize,
        .start = mem,
        .current = mem,
        .max_clear_size = 0,
        .average_clear_size = 0,
        .clear_count = 0
    };

    return allocator;
}


void *lgdb_lnmalloc(lgdb_linear_allocator_t *allocator, uint32_t size) {
    void *p = allocator->current;
    allocator->current = (void *)((uint8_t *)(allocator->current)+size);
    return p;
}


void lgdb_lndiagnostic_print(lgdb_linear_allocator_t *allocator, const char *allocator_name) {
    uint64_t size = (uint64_t)((uint8_t *)allocator->current - (uint8_t *)allocator->start);

    printf("LINEAR ALLOCATOR DIAGNOSTIC FOR: %s\n", allocator_name);
    printf("\t- capacity = %u bytes\n", allocator->max_size);
    printf("\t- current_usage = %llu bytes\n", size);
    printf("\t- max_historical_usage = %llu bytes\n", allocator->max_clear_size);
    printf("\t- average_historical_usage = %llu bytes\n", allocator->average_clear_size);
    printf("\t- clear_count = %llu\n", allocator->clear_count);
}


void lgdb_lnclear(lgdb_linear_allocator_t *allocator) {
    uint64_t size = (uint64_t)((uint8_t *)allocator->current - (uint8_t *)allocator->start);
    allocator->max_clear_size = MAX(size, allocator->max_clear_size);
    allocator->average_clear_size = (allocator->average_clear_size * allocator->clear_count + size) / (allocator->clear_count + 1);

    allocator->clear_count++;

    allocator->current = allocator->start;
}
