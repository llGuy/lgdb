#ifndef LGDB_ALLOC_H
#define LGDB_ALLOC_H

#include <stdint.h>

typedef struct lgdb_linear_allocator {
    void *start;
    void *current;
    uint32_t max_size;

    /* Just for diagnostics */
    uint64_t max_clear_size;
    uint64_t average_clear_size;
    uint64_t clear_count;
} lgdb_linear_allocator_t;

lgdb_linear_allocator_t lgdb_create_linear_allocator(uint32_t max_size);
void *lgdb_lnmalloc(lgdb_linear_allocator_t *allocator, uint32_t size);
void lgdb_lnclear(lgdb_linear_allocator_t *allocator);
void lgdb_lndiagnostic_print(lgdb_linear_allocator_t *allocator, const char *allocator_name);

inline uint64_t lgdb_kilobytes(uint32_t kb) {
    return kb * 1024;
}

inline uint64_t lgdb_megabytes(uint32_t mb) {
    return lgdb_kilobytes(mb * 1024);
}

#define LGDB_LNMALLOC(ptr_allocator, type, count) (type *)lgdb_lnmalloc(ptr_allocator, (uint32_t)(sizeof(type) * (count)))

#endif
