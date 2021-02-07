#ifndef LGDB_TABLE_H
#define LGDB_TABLE_H

#include <stdint.h>
#include "lgdb_utility.h"

/* Just a table which takes a unique integer, and maps it to another integer */
typedef struct lgdb_entry {
    uint32_t is_initialised : 1;
    uint32_t key : 31;
    /* Maybe make this 64bit */
    int32_t value;
    /* In the same bucket */
    lgdb_handle_t next_entry;
} lgdb_entry_t;

typedef struct lgdb_table {
    uint32_t bucket_count;
    uint32_t max_entries;
    lgdb_handle_t *buckets;
    lgdb_entry_t *pool;
    uint32_t pool_ptr;
} lgdb_table_t;

lgdb_table_t lgdb_create_table(uint32_t bucket_count, uint32_t max_entries);
void lgdb_clear_table(lgdb_table_t *table);
/* Raw key = 32 bit key. The actual key is 31 bits */
bool32_t lgdb_insert_in_table(lgdb_table_t *table, uint32_t raw_key, int32_t value);

#endif
