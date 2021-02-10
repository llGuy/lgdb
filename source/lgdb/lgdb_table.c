#include <assert.h>
#include "lgdb_table.h"


static lgdb_handle_t s_allocate_entry(lgdb_table_t *table) {
    assert(table->pool_ptr < table->max_entries);
    lgdb_handle_t hdl = table->pool_ptr++;

    table->pool[hdl].is_initialised = 0;
    table->pool[hdl].next_entry = LGDB_INVALID_HANDLE;

    return hdl;
}


static uint32_t s_extract_actual_key(uint32_t raw_key) {
    union {
        struct {
            uint32_t key0 : 16;
            uint32_t b : 1;
            uint32_t key1 : 15;
        };
        uint32_t bits;
    } s;

    s.bits = raw_key;

    return s.key0 + s.key1 << 15;
}


static inline lgdb_entry_t *s_get_entry(lgdb_table_t *table, lgdb_handle_t hdl) {
    return &table->pool[hdl];
}


lgdb_table_t lgdb_create_table(uint32_t bucket_count, uint32_t max_entries) {
    lgdb_table_t table = {
        .bucket_count = bucket_count,
        .max_entries = max_entries,
        .buckets = (lgdb_handle_t *)malloc(sizeof(lgdb_handle_t) * bucket_count),
        .pool = (lgdb_entry_t *)malloc(sizeof(lgdb_entry_t) * max_entries),
        .pool_ptr = 0
    };

    lgdb_clear_table(&table);

    return table;
}


void lgdb_free_table(lgdb_table_t *table) {
    free(table->buckets);
    free(table->pool);
}


void lgdb_clear_table(lgdb_table_t *table) {
    for (uint32_t i = 0; i < table->bucket_count; ++i) {
        table->buckets[i] = LGDB_INVALID_HANDLE;
    }

    memset(table->pool, 0, sizeof(lgdb_entry_t) * table->max_entries);

    table->pool_ptr = 0;
}


bool32_t lgdb_insert_in_table(lgdb_table_t *table, uint32_t raw_key, lgdb_entry_value_t value) {
    uint32_t actual_key = s_extract_actual_key(raw_key);
    uint32_t bucket_idx = actual_key % table->bucket_count;

    printf("Generated %d\n", actual_key);

    lgdb_handle_t *entry_p = &table->buckets[bucket_idx];

    if (*entry_p == LGDB_INVALID_HANDLE) {
        /* Need to initialise this bucket */
        *entry_p = s_allocate_entry(table);
        lgdb_entry_t *entry = s_get_entry(table, *entry_p);
        entry->is_initialised = 1;
        entry->key = actual_key;
        entry->value = value;
        entry->next_entry = LGDB_INVALID_HANDLE;

        return 1;
    }
    else {
        lgdb_entry_t *current_entry = s_get_entry(table, *entry_p);

        for (
            ; 
            current_entry->next_entry != LGDB_INVALID_HANDLE;
            current_entry = s_get_entry(table, current_entry->next_entry)) {
            if (!current_entry->is_initialised) {
                /* This entry was removed */
                current_entry->is_initialised = 1;
                current_entry->key = actual_key;
                current_entry->value = value;
                /* Next entry is already initialised */

                return 1;
            }
            else if (current_entry->key == actual_key) {
                /* Element was already added */
                return 0;
            }
        }

        if (current_entry->key == actual_key && current_entry->is_initialised)
            return 0;

        current_entry->next_entry = s_allocate_entry(table);

        lgdb_entry_t *new_entry = s_get_entry(table, current_entry->next_entry);
        new_entry->is_initialised = 1;
        new_entry->key = actual_key;
        new_entry->value = value;
        new_entry->next_entry = LGDB_INVALID_HANDLE;

        return 1;
    }
}


bool32_t lgdb_insert_in_tables(lgdb_table_t *table, const char *str, lgdb_entry_value_t value) {
    return lgdb_insert_in_table(table, lgdb_hash_string(str), value);
}


bool32_t lgdb_insert_in_tablep(lgdb_table_t *table, void *p, lgdb_entry_value_t value) {
    return lgdb_insert_in_table(table, lgdb_hash_pointer(p), value);
}


lgdb_entry_value_t *lgdb_get_from_table(lgdb_table_t *table, uint32_t raw_key) {
    uint32_t actual_key = s_extract_actual_key(raw_key);
    uint32_t bucket_idx = actual_key % table->bucket_count;

    lgdb_handle_t entry_hdl = table->buckets[bucket_idx];

    while (entry_hdl != LGDB_INVALID_HANDLE) {
        lgdb_entry_t *entry = s_get_entry(table, entry_hdl);

        if (entry->is_initialised && entry->key == actual_key) {
            return &entry->value;
        }

        entry_hdl = entry->next_entry;
    }

    return NULL;
}


lgdb_entry_value_t *lgdb_get_from_tables(lgdb_table_t *table, const char *str) {
    return lgdb_get_from_table(table, lgdb_hash_string(str));
}


lgdb_entry_value_t *lgdb_get_from_tablep(lgdb_table_t *table, void *p) {
    return lgdb_get_from_table(table, lgdb_hash_pointer(p));
}


bool32_t lgdb_remove_from_table(lgdb_table_t *table, uint32_t raw_key) {
    uint32_t actual_key = s_extract_actual_key(raw_key);
    uint32_t bucket_idx = actual_key % table->bucket_count;

    lgdb_handle_t entry_hdl = table->buckets[bucket_idx];

    while (entry_hdl != LGDB_INVALID_HANDLE) {
        lgdb_entry_t *entry = s_get_entry(table, entry_hdl);

        if (entry->is_initialised && entry->key == actual_key) {
            entry->is_initialised = 0;
            return 1;
        }

        entry_hdl = entry->next_entry;
    }

    return 0;
}


bool32_t lgdb_remove_from_tables(lgdb_table_t *table, const char *str) {
    return lgdb_remove_from_table(table, lgdb_hash_string(str));
}


bool32_t lgdb_remove_from_tablep(lgdb_table_t *table, void *p) {
    return lgdb_remove_from_table(table, lgdb_hash_pointer(p));
}
