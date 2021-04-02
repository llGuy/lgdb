#pragma once

#include <vector>
#include "swap.hpp"

extern "C" {

#include <lgdb_context.h>
#include <lgdb_symbol.h>

}

class popup_t;
class var_swapchain_t;
class variable_info_t;

struct variable_tick_info_t {
    lgdb_process_ctx_t *ctx;
    lgdb_linear_allocator_t *info_alloc;
    var_swapchain_t *copy;
    const char *sym_name;
    const char *type_name;
    copy_offset_t address;
    uint32_t size;
    lgdb_symbol_type_t *type;
    std::vector<popup_t *> *popups;
    std::vector<variable_info_t *> *modified_vars;
};

class variable_info_t {

public:

    void init(
        const char *name,
        lgdb_linear_allocator_t *info_alloc,
        var_swapchain_t *copy_alloc,
        lgdb_symbol_t *sym);

    void deep_sync(
        lgdb_process_ctx_t *ctx,
        lgdb_linear_allocator_t *info_alloc,
        var_swapchain_t *copy_alloc,
        std::vector<variable_info_t *> *modified_vars);

    bool tick(variable_tick_info_t *tinfo);

private:

    void deep_sync_pointer(
        lgdb_process_ctx_t *ctx,
        lgdb_linear_allocator_t *info_alloc,
        var_swapchain_t *copy_alloc,
        std::vector<variable_info_t *> *modified_vars,
        lgdb_symbol_type_t *type);

    void deep_sync_udt(
        lgdb_process_ctx_t *ctx,
        lgdb_linear_allocator_t *info_alloc,
        var_swapchain_t *copy_alloc,
        std::vector<variable_info_t *> *modified_vars,
        lgdb_symbol_type_t *type);

    // Will always be at the end of the recursive loops
    bool tick_symbol_base_type_data(variable_tick_info_t *tinfo);

    // Needs to be something like: [name] | {...} | [type] | [size]
    bool render_composed_var_row(
        const char *name,
        const char *type,
        uint32_t size);

    bool tick_udt(variable_tick_info_t *tinfo);
    bool tick_array(variable_tick_info_t *tinfo);
    bool tick_pointer(variable_tick_info_t *tinfo);
    bool tick_enum(variable_tick_info_t *tinfo);

public: // Temporarily public
    
    uint32_t open : 1;
    // Has the memory that this symbol encapsulates been updated
    uint32_t updated_memory : 1;
    uint32_t modified : 1;
    uint32_t inspecting_count : 15;
    uint32_t requested : 14;
    int32_t count_in_buffer;

    lgdb_symbol_t sym;
    variable_info_t *next;
    variable_info_t *next_open;
    variable_info_t *sub_start;
    variable_info_t *first_sub_open;
    variable_info_t *sub_end;

};
