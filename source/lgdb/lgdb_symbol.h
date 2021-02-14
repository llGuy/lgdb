#ifndef LGDB_SYMBOL_H
#define LGDB_SYMBOL_H

#include <stdint.h>
#include <Windows.h>
#include <DbgHelp.h>
#include "lgdb_utility.h"
#include "lgdb_table.h"
#include "lgdb_alloc.h"

#define LGDB_MAX_LOADED_DATA_SYMBOLS 10000

typedef struct lgdb_module_symbols {
    const char *path;
} lgdb_module_symbols_t;

typedef struct lgdb_symbol_data {
    uint64_t size : 32;
    uint64_t type_tag : 6;
    uint64_t base_type : 6;
    uint64_t children_count : 12;

    /* Actual value */
    union {
        float f32;
        double f64;

        uint8_t u8;
        int8_t s8;

        uint16_t u16;
        int16_t s16;

        uint32_t u32;
        int32_t s32;

        uint64_t u64;
        int64_t s64;

        uint64_t bits;
    } value;
} lgdb_symbol_data_t;

/* In future, find way to represent the value(s) stored in the symbol */
typedef struct lgdb_symbol {
    uint32_t sym_index;
    uint32_t type_index;
    uint32_t data_tag;

    /* For now, just support data symbols */
    lgdb_symbol_data_t data;
} lgdb_symbol_t;

typedef struct lgdb_process_symbols {
    /* 
        Contains more detailed information about the symbols of the modules 
        for instance some sort of line number information cache.
    */
    uint32_t module_count;
    lgdb_module_symbols_t *modules;

    lgdb_handle_t root_type_node;

    /* Get the data of the symbol from the DebugHlp index of the symbol */
    lgdb_table_t sym_name_to_ptr;

    /* Only clear this when changing scope or something */
    lgdb_linear_allocator_t data_mem;
} lgdb_process_symbols_t;

/* Just some utility functions */
IMAGEHLP_SYMBOL64 *lgdb_make_symbol_info(struct lgdb_process_ctx *ctx, const char *symbol_name);
IMAGEHLP_LINE64 lgdb_make_line_info(struct lgdb_process_ctx *ctx, const char *file_name, uint32_t line_number);
IMAGEHLP_LINE64 lgdb_make_line_info_from_addr(struct lgdb_process_ctx *ctx, void *addr);
IMAGEHLP_LINE64 lgdb_get_next_line_info(struct lgdb_process_ctx *ctx, IMAGEHLP_LINE64 line);
void lgdb_update_symbol_context(struct lgdb_process_ctx *ctx);
void lgdb_update_local_symbols(struct lgdb_process_ctx *ctx);
void lgdb_get_symbol_value(struct lgdb_process_ctx *ctx, SYMBOL_INFO *info, lgdb_symbol_t *dst);
lgdb_symbol_t *lgdb_get_registered_symbol(struct lgdb_process_ctx *ctx, const char *name);

#endif