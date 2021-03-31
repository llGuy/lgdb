#pragma once

extern "C" {

#include <lgdb_context.h>
#include <lgdb_symbol.h>

}

class variable_info_t {

public:

    void init(
        const char *name,
        lgdb_linear_allocator_t *info_alloc,
        lgdb_linear_allocator_t *copy_alloc,
        lgdb_symbol_t *sym);

    void deep_sync(
        lgdb_process_ctx_t *ctx,
        lgdb_linear_allocator_t *info_alloc,
        lgdb_linear_allocator_t *copy_alloc);

    void tick(
        const char *sym_name,
        const char *type_name,
        void *address,
        uint32_t size,
        lgdb_symbol_type_t *type);

private:

    void deep_sync_pointer(
        lgdb_process_ctx_t *ctx,
        lgdb_linear_allocator_t *info_alloc,
        lgdb_linear_allocator_t *copy_alloc,
        lgdb_symbol_type_t *type);

    void deep_sync_udt(
        lgdb_process_ctx_t *ctx,
        lgdb_linear_allocator_t *info_alloc,
        lgdb_linear_allocator_t *copy_alloc,
        lgdb_symbol_type_t *type);

    // Will always be at the end of the recursive loops
    bool tick_symbol_base_type_data(
        const char *name,
        const char *type_name
        void *address,
        uint32_t size,
        lgdb_symbol_type_t *type);

    // Needs to be something like: [name] | {...} | [type] | [size]
    bool render_composed_var_row(
        const char *name,
        const char *type,
        uint32_t size);

    bool tick_udt(
        const char *sym_name,
        const char *type_name,
        void *address,
        uint32_t size,
        lgdb_symbol_type_t *type);

    bool tick_array(
        const char *sym_name,
        const char *type_name,
        void *address,
        uint32_t size,
        lgdb_symbol_type_t *type);

public: // Temporarily public
    
    uint32_t open : 1;
    // Has the memory that this symbol encapsulates been updated
    uint32_t updated_memory : 1;
    uint32_t inspecting_count : 15;
    uint32_t requested : 15;
    int32_t count_in_buffer;

    lgdb_symbol_t sym;
    variable_info_t *next;
    variable_info_t *next_open;
    variable_info_t *sub_start;
    variable_info_t *first_sub_open;
    variable_info_t *sub_end;

};
