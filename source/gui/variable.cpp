#define _NO_CVCONST_H

#include <Windows.h>
#include <DbgHelp.h>
#include <TypeInfoStructs.h>
#include "variable.hpp"


void variable_info_t::init(
    const char *name,
    lgdb_linear_allocator_t *info_alloc,
    lgdb_linear_allocator_t *copy_alloc,
    lgdb_symbol_t *symbol) {
    sym = *symbol;
    open = 0;

    uint32_t name_len = (uint32_t)strlen(name);
    sym.name = (char *)lgdb_lnmalloc(info_alloc, (1 + name_len) * sizeof(char));
    memcpy(sym.name, name, name_len * sizeof(char));
    sym.name[name_len] = 0;

    void *data = lgdb_lnmalloc(copy_alloc, sym.size);
    sym.debugger_bytes_ptr = data;
    sym.user_flags = 0;

    sub_start = NULL;
}


void variable_info_t::deep_sync_pointer(
    lgdb_process_ctx_t *ctx,
    lgdb_linear_allocator_t *info_alloc,
    lgdb_linear_allocator_t *copy_alloc,
    lgdb_symbol_type_t *type) {
    // Priority 1: read the value of the pointer itself
    if (!updated_memory) {
        lgdb_read_buffer_from_process(
            ctx,
            (uint64_t)lgdb_get_real_symbol_address(ctx, &sym),
            sym.size,
            sym.debugger_bytes_ptr);
    }

    updated_memory = 0;

    // Priority 2: read the array of values that are being pointed
    if (open) {
        lgdb_symbol_type_t *pointed_type = lgdb_get_type(ctx, type->uinfo.pointer_type.type_index);
        const uint64_t pointer_value = *(uint64_t *)sym.debugger_bytes_ptr;

        if (!sub_start) {
            sub_start = (variable_info_t *)lgdb_lnmalloc(
                info_alloc,
                sizeof(variable_info_t) * requested);

            memset(sub_start, 0, sizeof(variable_info_t) * requested);

            { // Initialise the linked list
                auto *sub = sub_start;
                sub->count_in_buffer = requested;
                sub->sym.real_addr = pointer_value;
                sub->sym.type_index = pointed_type->index;

                sub_end = sub + (requested - 1);

                uint64_t current_ptr = pointer_value + pointed_type->size;
                for (uint32_t i = 1; i < requested; ++i) {
                    sub->sym.real_addr = current_ptr;
                    sub->sym.type_index = pointed_type->index;
                    sub->count_in_buffer = -1;

                    sub->next = sub + 1;
                    sub = sub->next;
                    current_ptr += pointed_type->size;
                }

                sub->next = NULL;
            }

            /* Only read in all the values if this isn't a composed type */
            if (pointed_type->tag != SymTagUDT && pointed_type->tag != SymTagArrayType) {
                void *copy_buffer = lgdb_lnmalloc(
                    copy_alloc,
                    pointed_type->size * requested);

                lgdb_read_buffer_from_process(
                    ctx,
                    pointer_value,
                    pointed_type->size * requested,
                    copy_buffer);

                auto *sub = sub_start;
                for (uint32_t i = 0; i < requested; ++i) {
                    sub[i].sym.debugger_bytes_ptr = (uint8_t *)copy_buffer + pointed_type->size * i;
                }
            }

            inspecting_count = requested;
            requested = 0;
        }
        else if (requested > inspecting_count) {
            // We need to allocate a new buffer
            /* Add some more */
            uint32_t diff = requested - inspecting_count;
            auto *original_sub_end = sub_end->next = (variable_info_t *)lgdb_lnmalloc(info_alloc, sizeof(variable_info_t) * diff);
            memset(sub_end->next, 0, sizeof(variable_info_t) * diff);

            { // Initialise the linked list from here
                uint64_t current_ptr = pointer_value + inspecting_count * pointed_type->size;

                auto *sub = sub_end->next, *start = sub_end->next;
                sub_end = sub + (diff - 1);

                for (uint32_t i = 0; i < diff; ++i) {
                    sub->sym.real_addr = current_ptr;
                    sub->sym.type_index = pointed_type->index;
                    sub->count_in_buffer = -1;

                    sub->next = sub + 1;
                    sub = sub->next;

                    current_ptr += pointed_type->size;
                }

                start->count_in_buffer = diff;
                sub_end->next = NULL;
            }

            if (pointed_type->tag != SymTagUDT && pointed_type->tag != SymTagArrayType) {
                void *copy_buffer = lgdb_lnmalloc(
                    copy_alloc,
                    pointed_type->size * diff);

                lgdb_read_buffer_from_process(
                    ctx,
                    pointer_value + inspecting_count * pointed_type->size,
                    pointed_type->size * diff,
                    copy_buffer);

                auto *sub = original_sub_end;
                for (uint32_t i = 0; i < diff; ++i) {
                    sub[i].sym.debugger_bytes_ptr = (uint8_t *)copy_buffer + pointed_type->size * i;
                }
            }

            inspecting_count = requested;
            requested = 0;
        }
        else {
            if (pointed_type->tag != SymTagUDT && pointed_type->tag != SymTagArrayType) {
                uint32_t i = 0;
                auto *sub = sub_start;

                while (i < inspecting_count) {
                    int32_t count_in_buffer = sub->count_in_buffer;
                    if (count_in_buffer != -1) {
                        lgdb_read_buffer_from_process(
                            ctx,
                            pointer_value + i * pointed_type->size,
                            pointed_type->size * count_in_buffer,
                            sub->sym.debugger_bytes_ptr);

                        auto *sub_end_in_buffer = sub + (count_in_buffer - 1);
                        sub = sub_end_in_buffer->next;

                        i += count_in_buffer;
                    }
                    else {
                        ++i;
                    }
                }
            }
        }
    }
}


void variable_info_t::deep_sync_udt(
    lgdb_process_ctx_t *ctx,
    lgdb_linear_allocator_t *info_alloc,
    lgdb_linear_allocator_t *copy_alloc,
    lgdb_symbol_type_t *type) {
    if (open) {
        uint64_t process_addr = (uint64_t)lgdb_get_real_symbol_address(ctx, &sym);

        if (!updated_memory) {
            lgdb_read_buffer_from_process(
                ctx,
                process_addr,
                sym.size,
                sym.debugger_bytes_ptr);
        }

        updated_memory = 0;

        if (!sub_start) {
            // Allocate sub variable_infos
            uint32_t base_class_count = type->uinfo.udt_type.base_classes_count;
            uint32_t member_var_count = type->uinfo.udt_type.member_var_count;
            uint32_t sub_count = base_class_count + member_var_count;

            variable_info_t *current_sub = sub_start =
                (variable_info_t *)lgdb_lnmalloc(info_alloc, sizeof(variable_info_t) * sub_count);

            uint8_t *current_dbg_ptr = (uint8_t *)sym.debugger_bytes_ptr;
            uint64_t current_process_addr = process_addr;

            for (uint32_t i = 0; i < base_class_count; ++i) {
                lgdb_base_class_t *base_class = &type->uinfo.udt_type.base_classes_type_idx[i];

                current_sub[i].count_in_buffer = -1;
                current_sub[i].sym.real_addr;

                current_dbg_ptr += base_class->size;
                current_process_addr = base_class->size;
            }
        }
    }
}


void variable_info_t::deep_sync(
    lgdb_process_ctx_t *ctx,
    lgdb_linear_allocator_t *info_alloc,
    lgdb_linear_allocator_t *copy_alloc) {
    lgdb_symbol_type_t *type = lgdb_get_type(ctx, sym.type_index);

    switch (type->tag) {
    case SymTagUDT: {
        deep_sync_udt(ctx, info_alloc, copy_alloc, type);
    } break;

    case SymTagPointerType: {
        deep_sync_pointer(ctx, info_alloc, copy_alloc, type);
    } break;

    default: {
        lgdb_read_buffer_from_process(
            ctx,
            (uint64_t)lgdb_get_real_symbol_address(ctx, &sym),
            sym.size,
            sym.debugger_bytes_ptr);
    };
    }
}


void variable_info_t::tick() {

}
