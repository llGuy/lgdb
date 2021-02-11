#ifndef LGDB_TABLE_H
#define LGDB_TABLE_H

#include <stdint.h>
#include "lgdb_utility.h"

/* Typedefed because might want to change this to 64bit */
typedef int32_t lgdb_entry_value_t;

/* Just a table which takes a unique integer, and maps it to another integer */
typedef struct lgdb_entry {
    uint32_t key;
    lgdb_entry_value_t value;

    /* In the same bucket */
    uint32_t is_initialised : 1;
    uint32_t next_entry : 31;
} lgdb_entry_t;

typedef struct lgdb_table {
    uint32_t bucket_count;
    uint32_t max_entries;
    lgdb_handle_t *buckets;
    lgdb_entry_t *pool;
    uint32_t pool_ptr;
} lgdb_table_t;

lgdb_table_t lgdb_create_table(uint32_t bucket_count, uint32_t max_entries);
void lgdb_free_table(lgdb_table_t *table);
void lgdb_clear_table(lgdb_table_t *table);
/* Raw key = 32 bit key. The actual key is 31 bits */
bool32_t lgdb_insert_in_table(lgdb_table_t *table, uint32_t raw_key, lgdb_entry_value_t value);
/* Insert with string */
bool32_t lgdb_insert_in_tables(lgdb_table_t *table, const char *str, lgdb_entry_value_t value);
bool32_t lgdb_insert_in_tablep(lgdb_table_t *table, void *p, lgdb_entry_value_t value);
/* NULL is returned if no value was found */
lgdb_entry_value_t *lgdb_get_from_table(lgdb_table_t *table, uint32_t raw_key);
lgdb_entry_value_t *lgdb_get_from_tables(lgdb_table_t *table, const char *str);
lgdb_entry_value_t *lgdb_get_from_tablep(lgdb_table_t *table, void *p);
bool32_t lgdb_remove_from_table(lgdb_table_t *table, uint32_t raw_key);
bool32_t lgdb_remove_from_tables(lgdb_table_t *table, const char *str);
bool32_t lgdb_remove_from_tablep(lgdb_table_t *table, void *p);

#endif
