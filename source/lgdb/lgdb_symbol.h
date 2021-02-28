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

typedef struct lgdb_symbol_base_type {
    uint32_t base_type;
} lgdb_symbol_base_type_t;

/* Most of these just point to other types */
typedef struct lgdb_symbol_typedef_type {
    uint32_t type_index;
} lgdb_symbol_typedef_type_t;

typedef struct lgdb_symbol_pointer_type {
    uint32_t type_index;
} lgdb_symbol_pointer_type_t;

typedef struct lgdb_symbol_array_type {
    uint32_t type_index;
} lgdb_symbol_array_type_t;

typedef struct lgdb_member_var {
    uint32_t sym_idx;
    uint32_t type_idx;
    uint32_t offset;
    uint32_t size;
} lgdb_member_var_t;

/* Basically the same as lgdb_member_var_t... *sigh* */
typedef struct lgdb_base_class {
    uint32_t idx;
    uint32_t type_idx;
    uint32_t offset;
    uint32_t size;
} lgdb_base_class_t;

/* User-defined type */
typedef struct lgdb_symbol_udt_type {
    uint32_t udt_kind;
    uint32_t children_count;
    /* Pointer to a (potential) bunch of type indices of the member types */
    uint32_t member_var_count;
    lgdb_member_var_t *member_vars_type_idx;

    uint32_t methods_count;
    uint32_t *methods_type_idx;

    uint32_t base_classes_count;
    /* Also contains the offset */
    lgdb_base_class_t *base_classes_type_idx;
} lgdb_symbol_udt_type_t;

/* Function type */
typedef struct lgdb_symbol_function_type {
    uint32_t return_type_index;
    uint32_t arg_count;
    /* The type indices of the arguments */
    uint32_t *arg_types;
    /* If this is a member function */
    uint32_t class_id; // Set to 0xFFFFFFFF if it's not member function
    uint32_t this_adjust;
} lgdb_symbol_function_type_t;

typedef struct lgdb_symbol_enum_type {
    uint32_t type_index;
    /* Other things */

    lgdb_table_t value_to_name;
} lgdb_symbol_enum_type_t;

typedef struct lgdb_symbol_type {
    uint32_t index;
    uint32_t tag;
    uint32_t size;

    union {
        lgdb_symbol_enum_type_t enum_type;

        lgdb_symbol_base_type_t base_type;
        lgdb_symbol_typedef_type_t typedef_type;
        lgdb_symbol_pointer_type_t pointer_type;
        lgdb_symbol_array_type_t array_type;
        lgdb_symbol_udt_type_t udt_type;
        lgdb_symbol_function_type_t function_type;
    } uinfo;
} lgdb_symbol_type_t;

/* In future, find way to represent the value(s) stored in the symbol */
typedef struct lgdb_symbol {
    uint32_t size;
    uint32_t sym_index;
    uint32_t sym_tag;
    uint32_t sym_size;
    uint32_t type_index;
    /* In debuggee address space */
    uint64_t start_addr;

    /* Pointer in debugger address space */
    void *debugger_bytes_ptr;
} lgdb_symbol_t;

typedef void (*lgdb_update_symbol_proc_t)(struct lgdb_process_ctx *ctx, const char *name, lgdb_symbol_t *sym);

typedef struct lgdb_process_symbols {
    /* 
        Contains more detailed information about the symbols of the modules 
        for instance some sort of line number information cache.
    */
    uint32_t module_count;
    lgdb_module_symbols_t *modules;

    /* TODO: Make sure that this doesn't map to a pointer but an index into the symbol_ptr_pool */
    lgdb_table_t sym_name_to_ptr;
    lgdb_table_t type_idx_to_ptr;

    uint32_t data_symbol_count;
    /* Just a pool of memory where we can push the pointers to all the data symbols */
    lgdb_symbol_t **symbol_ptr_pool;

    /* Will need to get rid of this in the future */
    /* Only clear this when changing scope or something */
    lgdb_linear_allocator_t data_mem;
    /* Never gets cleared until another process gets loaded */
    lgdb_linear_allocator_t type_mem;
    /* Where the copy of the data is stored */
    lgdb_linear_allocator_t copy_mem;

    lgdb_update_symbol_proc_t current_updt_sym_proc;
} lgdb_process_symbols_t;

/* Just some utility functions */
IMAGEHLP_SYMBOL64 *lgdb_make_symbol_info(struct lgdb_process_ctx *ctx, const char *symbol_name);
const char *lgdb_get_containing_file(struct lgdb_process_ctx *ctx, const char *symbol_name);
IMAGEHLP_LINE64 lgdb_make_line_info(struct lgdb_process_ctx *ctx, const char *file_name, uint32_t line_number);
IMAGEHLP_LINE64 lgdb_make_line_info_from_addr(struct lgdb_process_ctx *ctx, void *addr);
IMAGEHLP_LINE64 lgdb_get_next_line_info(struct lgdb_process_ctx *ctx, IMAGEHLP_LINE64 line);
void lgdb_update_symbol_context(struct lgdb_process_ctx *ctx);

/* To deprecate!! */
void lgdb_update_local_symbols_depr(struct lgdb_process_ctx *ctx);

void lgdb_update_local_symbols(struct lgdb_process_ctx *ctx, lgdb_update_symbol_proc_t proc);

lgdb_symbol_type_t *lgdb_get_type(struct lgdb_process_ctx *ctx, uint32_t type_index);
lgdb_symbol_t *lgdb_get_registered_symbol(struct lgdb_process_ctx *ctx, const char *name);
void lgdb_print_symbol_value(struct lgdb_process_ctx *ctx, const char *name);

#endif
