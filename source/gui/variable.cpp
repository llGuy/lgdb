#define _NO_CVCONST_H
#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <DbgHelp.h>
#include <TypeInfoStructs.h>
#include "variable.hpp"

#include <imgui.h>
#include <stdio.h>
#include "debugger.hpp"


void variable_info_t::init(
    const char *name,
    lgdb_linear_allocator_t *info_alloc,
    var_swapchain_t *copy_alloc,
    lgdb_symbol_t *symbol) {
    sym = *symbol;
    open = 0;

    uint32_t name_len = (uint32_t)strlen(name);
    sym.name = (char *)lgdb_lnmalloc(info_alloc, (1 + name_len) * sizeof(char));
    memcpy(sym.name, name, name_len * sizeof(char));
    sym.name[name_len] = 0;

    sym.dbg_offset = copy_alloc->allocate(sym.size);
    sym.user_flags = 0;

    first_sub_open = NULL;
    sub_start = NULL;
    modified = 0;
}


void variable_info_t::deep_sync_pointer(
    lgdb_process_ctx_t *ctx,
    lgdb_linear_allocator_t *info_alloc,
    var_swapchain_t *copy_alloc,
    std::vector<variable_info_t *> *modified_vars,
    lgdb_symbol_type_t *type) {
    void *dbg_ptr = copy_alloc->get_address(sym.dbg_offset);

    // Priority 1: read the value of the pointer itself
    if (!updated_memory) {
        lgdb_read_buffer_from_process(
            ctx,
            (uint64_t)lgdb_get_real_symbol_address(ctx, &sym),
            sym.size,
            dbg_ptr);
    }

    updated_memory = 0;

    const uint64_t pointer_value = *(uint64_t *)dbg_ptr;
    const uint64_t previous_pointer_value = *(uint64_t *)copy_alloc->get_address_prev(sym.dbg_offset);

    if (pointer_value != previous_pointer_value) {
        modified = 1;
        modified_vars->push_back(this);
    }

    // Priority 2: read the array of values that are being pointed
    if (open) {
        lgdb_symbol_type_t *pointed_type = lgdb_get_type(ctx, type->uinfo.pointer_type.type_index);

        if (!sub_start) {
            sub_start = (variable_info_t *)lgdb_lnmalloc(
                info_alloc,
                sizeof(variable_info_t) * requested);

            memset(sub_start, 0, sizeof(variable_info_t) * requested);

            { // Initialise the linked list
                auto *sub = sub_start;
                sub->count_in_buffer = requested;
                sub->sym.real_addr = pointer_value;
                sub->sym.size = pointed_type->size;
                sub->sym.type_index = pointed_type->index;

                sub_end = sub + (requested - 1);

                uint64_t current_ptr = pointer_value + pointed_type->size;
                for (uint32_t i = 1; i < requested; ++i) {
                    sub->sym.real_addr = current_ptr;
                    sub->sym.type_index = pointed_type->index;
                    sub->count_in_buffer = -1;
                    sub->sym.size = pointed_type->size;

                    sub->next = sub + 1;
                    sub = sub->next;
                    current_ptr += pointed_type->size;
                }

                sub->next = NULL;
            }

            /* Only read in all the values if this isn't a composed type */
            if (pointed_type->tag != SymTagUDT && pointed_type->tag != SymTagArrayType) {
                copy_offset_t copy_offset = copy_alloc->allocate(pointed_type->size * requested);
                void *copy_buffer = copy_alloc->get_address(copy_offset);

                lgdb_read_buffer_from_process(
                    ctx,
                    pointer_value,
                    pointed_type->size * requested,
                    copy_buffer);

                auto *sub = sub_start;
                for (uint32_t i = 0; i < requested; ++i) {
                    sub[i].sym.dbg_offset = copy_offset + pointed_type->size * i;
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
                    sub->sym.size = pointed_type->size;
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
                copy_offset_t copy_offset = copy_alloc->allocate(pointed_type->size * diff);
                void *copy_buffer = copy_alloc->get_address(copy_offset);

                lgdb_read_buffer_from_process(
                    ctx,
                    pointer_value + inspecting_count * pointed_type->size,
                    pointed_type->size * diff,
                    copy_buffer);

                auto *sub = original_sub_end;
                for (uint32_t i = 0; i < diff; ++i) {
                    sub[i].sym.dbg_offset = copy_offset + pointed_type->size * i;
                }
            }

            inspecting_count = requested;
            requested = 0;
        }
        else {
            // If the address has changed, we need to completely change everything...
            if (modified) {
                uint64_t current_pointer_value = pointer_value;
                for (auto *sub = sub_start; sub; sub = sub->next) {
                    sub->sym.real_addr = current_pointer_value;
                    current_pointer_value += sub->sym.size;
                }
            }

            if (pointed_type->tag == SymTagUDT || pointed_type->tag == SymTagArrayType) {
                for (
                    auto *current_open = first_sub_open;
                    current_open;
                    current_open = current_open->next_open) {
                    current_open->deep_sync(ctx, info_alloc, copy_alloc, modified_vars);
                }
            }
            else {
                uint32_t i = 0;
                auto *sub = sub_start;

                while (i < inspecting_count) {
                    int32_t count_in_buffer = sub->count_in_buffer;
                    if (count_in_buffer != -1) {
                        lgdb_read_buffer_from_process(
                            ctx,
                            pointer_value + i * pointed_type->size,
                            pointed_type->size * count_in_buffer,
                            copy_alloc->get_address(sub->sym.dbg_offset));

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
    var_swapchain_t *copy_alloc,
    std::vector<variable_info_t *> *modified_vars,
    lgdb_symbol_type_t *type) {
    if (open) {
        uint64_t process_addr = (uint64_t)lgdb_get_real_symbol_address(ctx, &sym);

        if (!sym.dbg_offset) {
            sym.dbg_offset = copy_alloc->allocate(sym.size);
        }

        void *dbg_ptr = copy_alloc->get_address(sym.dbg_offset);

        if (!updated_memory) {
            lgdb_read_buffer_from_process(
                ctx,
                process_addr,
                sym.size,
                dbg_ptr);
        }

        updated_memory = 0;

        if (sub_start) {
            // Update all the sub variables
            assert(count_in_buffer != -1);
            for (uint32_t i = 0; i < (uint32_t)count_in_buffer; ++i) {

                auto *current_info = &sub_start[i];
                current_info->updated_memory = 1; // We don't want these vars to read from process memory

                if (current_info->sym.sym_tag == SymTagPointerType) {
                    const uint64_t pointer_value = *(uint64_t *)copy_alloc->get_address(sym.dbg_offset);
                    const uint64_t previous_pointer_value = *(uint64_t *)copy_alloc->get_address_prev(sym.dbg_offset);
                    if (pointer_value != previous_pointer_value) {
                        uint64_t current_pointer_value = pointer_value;
                        for (auto *sub = current_info->sub_start; sub; sub = sub->next) {
                            sub->sym.real_addr = current_pointer_value;
                            current_pointer_value += sub->sym.size;
                        }

                        modified = 1;
                        modified_vars->push_back(current_info);
                    }
                }

                current_info->deep_sync(ctx, info_alloc, copy_alloc, modified_vars);

            }
        }
        else {
            // Allocate sub variable_infos
            uint32_t base_class_count = type->uinfo.udt_type.base_classes_count;
            uint32_t member_var_count = type->uinfo.udt_type.member_var_count;
            uint32_t sub_count = base_class_count + member_var_count;
            count_in_buffer = sub_count;

            variable_info_t *current_sub = sub_start =
                (variable_info_t *)lgdb_lnmalloc(info_alloc, sizeof(variable_info_t) * sub_count);
            memset(current_sub, 0, sizeof(variable_info_t) * sub_count);

            copy_offset_t current_dbg_ptr = sym.dbg_offset;

            for (uint32_t i = 0; i < base_class_count; ++i) {
                lgdb_base_class_t *base_class = &type->uinfo.udt_type.base_classes_type_idx[i];

                current_sub[i].open = 0;
                current_sub[i].count_in_buffer = -1;
                current_sub[i].sym.real_addr = process_addr + base_class->offset;
                current_sub[i].sym.type_index = base_class->type_idx;
                current_sub[i].sym.size = base_class->size;
                current_sub[i].sym.dbg_offset = sym.dbg_offset + base_class->offset;
                current_sub[i].sym.user_flags = 0;
            }

            current_sub += base_class_count;

            for (uint32_t i = 0; i < member_var_count; ++i) {
                lgdb_member_var_t *member_var = &type->uinfo.udt_type.member_vars_type_idx[i];

                current_sub[i].open = 0;
                current_sub[i].count_in_buffer = -1;
                current_sub[i].sym.real_addr = process_addr + member_var->offset;
                current_sub[i].sym.type_index = member_var->type_idx;
                current_sub[i].sym.size = member_var->size;
                current_sub[i].sym.dbg_offset = sym.dbg_offset + member_var->offset;
                current_sub[i].sym.user_flags = 0;
            }
        }
    }
}


void variable_info_t::deep_sync(
    lgdb_process_ctx_t *ctx,
    lgdb_linear_allocator_t *info_alloc,
    var_swapchain_t *copy_alloc,
    std::vector<variable_info_t *> *modified_vars) {
    lgdb_symbol_type_t *type = lgdb_get_type(ctx, sym.type_index);

    switch (type->tag) {
    case SymTagUDT: {
        deep_sync_udt(ctx, info_alloc, copy_alloc, modified_vars, type);
    } break;

    case SymTagPointerType: {
        deep_sync_pointer(ctx, info_alloc, copy_alloc, modified_vars, type);
    } break;

    default: {
        void *current_addr = copy_alloc->get_address(sym.dbg_offset);
        void *prev_addr = copy_alloc->get_address_prev(sym.dbg_offset);
        if (!updated_memory) {
            lgdb_read_buffer_from_process(
                ctx,
                (uint64_t)lgdb_get_real_symbol_address(ctx, &sym),
                sym.size,
                copy_alloc->get_address(sym.dbg_offset));
        }

        if (memcmp(current_addr, prev_addr, sym.size) != 0) {
            modified = 1;
            modified_vars->push_back(this);
        }

        updated_memory = 0;
    };
    }
}


bool variable_info_t::tick_udt(variable_tick_info_t *tinfo) {
    bool opened = render_composed_var_row(tinfo->sym_name, tinfo->type->name, tinfo->size);

    if (!open && opened) {
        open = 1;
        requested = 1;
        deep_sync(tinfo->ctx, tinfo->info_alloc, tinfo->copy, tinfo->modified_vars);
    }

    if (opened) {
        variable_info_t *info = &sub_start[0];
        for (uint32_t i = 0; i < tinfo->type->uinfo.udt_type.base_classes_count; ++i) {
            lgdb_base_class_t *base_class = &tinfo->type->uinfo.udt_type.base_classes_type_idx[i];
            lgdb_symbol_type_t *base_class_type = lgdb_get_type(tinfo->ctx, base_class->type_idx);
            info->updated_memory = 1;

            variable_tick_info_t new_tinfo = *tinfo;
            new_tinfo.sym_name = base_class_type->name;
            new_tinfo.type_name = base_class_type->name;
            new_tinfo.address = info->sym.dbg_offset;
            new_tinfo.size = base_class->size;
            new_tinfo.type = base_class_type;

            info->tick(&new_tinfo);

            info++;
        }

        for (uint32_t i = 0; i < tinfo->type->uinfo.udt_type.member_var_count; ++i) {
            lgdb_member_var_t *member_var = &tinfo->type->uinfo.udt_type.member_vars_type_idx[i];
            lgdb_symbol_type_t *member_type = lgdb_get_type(tinfo->ctx, member_var->type_idx);
            info->updated_memory = 1;

            variable_tick_info_t new_tinfo = *tinfo;
            new_tinfo.sym_name = member_var->name;
            new_tinfo.type_name = member_type->name;
            new_tinfo.address = info->sym.dbg_offset;
            new_tinfo.size = member_var->size;
            new_tinfo.type = member_type;

            info->tick(&new_tinfo);

            info++;
        }

        ImGui::TreePop();
    }

    return opened;
}


bool variable_info_t::tick_array(variable_tick_info_t *tinfo) {
    lgdb_symbol_type_t *arrayed_type = lgdb_get_type(tinfo->ctx, tinfo->type->uinfo.array_type.type_index);

    uint32_t element_count = tinfo->size / arrayed_type->size;

    const char *arrayed_type_name = arrayed_type->name;
    if (arrayed_type->tag == SymTagBaseType) {
        arrayed_type_name = lgdb_get_base_type_string(arrayed_type->uinfo.base_type.base_type);
    }

    char buffer[50] = {};
    sprintf(buffer, "%s[%d]", arrayed_type_name, element_count);

    bool opened = render_composed_var_row(tinfo->sym_name, buffer, tinfo->size);

    if (opened) {
        for (uint32_t i = 0; i < element_count && i < 30; ++i) {
            sprintf(buffer, "[%d]", i);

            variable_tick_info_t new_tinfo = *tinfo;
            new_tinfo.sym_name = buffer;
            new_tinfo.type_name = arrayed_type->name;
            new_tinfo.address = tinfo->address + arrayed_type->size * i;
            new_tinfo.size = arrayed_type->size;
            new_tinfo.type = arrayed_type;

            this->tick(&new_tinfo);
        }

        ImGui::TreePop();
    }

    return opened;
}


bool variable_info_t::tick(variable_tick_info_t *tinfo) {
    switch (tinfo->type->tag) {
    case SymTagBaseType: {
        return tick_symbol_base_type_data(tinfo);
    }

    case SymTagTypedef: {
        lgdb_symbol_type_t *typedefed_type = lgdb_get_type(tinfo->ctx, tinfo->type->uinfo.typedef_type.type_index);
        variable_tick_info_t new_tinfo = *tinfo;
        new_tinfo.type = typedefed_type;
        return tick(&new_tinfo);
    }

    case SymTagUDT: {
        return tick_udt(tinfo);
    }

    case SymTagArrayType: {
        return tick_array(tinfo);
    }

    case SymTagEnum: {
        return tick_enum(tinfo);
    }

    case SymTagPointerType: {
        return tick_pointer(tinfo);
    }

    default: return 0;
    }
}


bool variable_info_t::tick_symbol_base_type_data(variable_tick_info_t *info) {
    const char *type_name_actual = info->type_name;
    if (!info->type_name)
        type_name_actual = lgdb_get_base_type_string(info->type->uinfo.base_type.base_type);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);

    uint32_t count = info->size / info->type->size;

    if (count == 1) {
        if (modified) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
            ImGui::TreeNodeEx(
                info->sym_name,
                ImGuiTreeNodeFlags_Leaf |
                ImGuiTreeNodeFlags_Bullet |
                ImGuiTreeNodeFlags_NoTreePushOnOpen |
                ImGuiTreeNodeFlags_SpanFullWidth);
            ImGui::PopStyleColor();
        }
        else {
            ImGui::TreeNodeEx(
                info->sym_name,
                ImGuiTreeNodeFlags_Leaf |
                ImGuiTreeNodeFlags_Bullet |
                ImGuiTreeNodeFlags_NoTreePushOnOpen |
                ImGuiTreeNodeFlags_SpanFullWidth);
        }

        ImGui::TableSetColumnIndex(1);

        if (info->size / info->type->size == 1) {
            void *address = info->copy->get_address(info->address);

            switch (info->type->uinfo.base_type.base_type) {
            case btChar: ImGui::Text("\'%c\'", *((char *)address)); break;
                // case btWChar:
            case btInt: ImGui::Text("%d", *((int *)address)); break;
            case btUInt: ImGui::Text("%d", *((unsigned int *)address)); break;
            case btFloat: ImGui::Text("%f", *((float *)address)); break;
            case btBool: ImGui::Text("%s", *((bool *)address) ? "true" : "false"); break;
            case btLong: ImGui::Text("%d", *((int *)address)); break;
            case btULong: ImGui::Text("%d", *((unsigned int *)address)); break;
            default: ImGui::Text("--"); break;
            }
        }
        else {
            ImGui::TextUnformatted("{...}");
        }

        ImGui::TableSetColumnIndex(2);
        ImGui::Text(type_name_actual);
        ImGui::TableNextColumn();
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%d", info->size);
    }
    else {
        assert(0);
    }

    // This will always return 0
    return 0;
}


bool variable_info_t::render_composed_var_row(
    const char *name,
    const char *type,
    uint32_t size) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);

    bool opened_struct = 0;
    if (modified) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        opened_struct = ImGui::TreeNodeEx(name, ImGuiTreeNodeFlags_SpanFullWidth);
        ImGui::PopStyleColor();
    }
    else {
        opened_struct = ImGui::TreeNodeEx(name, ImGuiTreeNodeFlags_SpanFullWidth);
    }

    if (ImGui::BeginPopupContextItem(NULL)) {
        if (ImGui::Selectable("Something Cool")) {
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::Selectable("Something not cool")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::TableSetColumnIndex(1);
    ImGui::TextDisabled("{...}");
    ImGui::TableSetColumnIndex(2);
    ImGui::TextUnformatted(type);
    ImGui::TableSetColumnIndex(3);
    ImGui::Text("%d", size);

    return opened_struct;
}


bool variable_info_t::tick_pointer(variable_tick_info_t *tinfo) {
    lgdb_symbol_type_t *pointed_type = lgdb_get_type(tinfo->ctx, tinfo->type->uinfo.pointer_type.type_index);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    bool opened = 0;

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth;

    if (modified) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        opened = ImGui::TreeNodeEx(tinfo->sym_name, ImGuiTreeNodeFlags_SpanFullWidth);
        ImGui::PopStyleColor();
    }
    else {
        opened = ImGui::TreeNodeEx(tinfo->sym_name, ImGuiTreeNodeFlags_SpanFullWidth);
    }

    if (ImGui::BeginPopupContextItem(NULL)) {
        if (ImGui::Selectable("Enter View Count")) {
            tinfo->popups->push_back(new popup_view_count_t(this));

            ImGui::CloseCurrentPopup();
        }

        if (ImGui::Selectable("Update (Debug)")) {
            deep_sync(tinfo->ctx, tinfo->info_alloc, tinfo->copy, tinfo->modified_vars);

            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    if (!open && opened) {
        open = 1;
        requested = 1;
        deep_sync(tinfo->ctx, tinfo->info_alloc, tinfo->copy, tinfo->modified_vars);
    }

    open = opened;
        
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%p", *(void **)(tinfo->copy->get_address(tinfo->address)));
    ImGui::TableSetColumnIndex(2);

    const char *pointed_type_name = pointed_type->name;
    if (pointed_type->tag == SymTagBaseType) {
        pointed_type_name = lgdb_get_base_type_string(pointed_type->uinfo.base_type.base_type);
    }

    ImGui::Text("%s*", pointed_type_name);
    ImGui::TableSetColumnIndex(3);
    ImGui::Text("%d", tinfo->size);

    if (opened) {
        auto *sub = sub_start;
        char name_buf[15] = {};

        uint32_t i = 0;
        variable_info_t *prev_open = first_sub_open;
        for (auto *sub = sub_start; sub; sub = sub->next) {
            lgdb_symbol_type_t *type = lgdb_get_type(tinfo->ctx, sub->sym.type_index);
            sprintf(name_buf, "[%d]", i);

            bool was_open = sub->open;

            variable_tick_info_t new_tinfo = *tinfo;
            new_tinfo.address = sub->sym.dbg_offset;
            new_tinfo.size = type->size;
            new_tinfo.type = type;
            new_tinfo.sym_name = name_buf;
            new_tinfo.type_name = type->name;

            bool opened_sub = sub->tick(&new_tinfo);

            bool need_to_update_openness = !was_open && opened_sub;

            if (opened_sub) {
                if (!was_open) {
                    sub->open = 1;

                    // The first element of the linked list hasn't been initialised
                    if (!prev_open) {
                        prev_open = first_sub_open = sub;
                    }
                    else {
                        variable_info_t *next_open = prev_open->next_open;

                        prev_open->next_open = sub;
                        sub->next_open = next_open;
                    }
                }

                prev_open = sub;
            }
            else {
                if (was_open) {
                    sub->open = 0;

                    if (first_sub_open == sub) {
                        first_sub_open = NULL;
                    }
                    else {
                        prev_open->next_open = sub->next_open;
                        sub->open = NULL;
                    }
                }
            }

            ++i;
        }

        ImGui::TreePop();
    }

    return opened;
}


bool variable_info_t::tick_enum(variable_tick_info_t *tinfo) {
    lgdb_symbol_type_t *enum_type = lgdb_get_type(tinfo->ctx, tinfo->type->uinfo.enum_type.type_index);

    int32_t bytes;

    void *address = tinfo->copy->get_address(tinfo->address);

    switch (tinfo->size) {
    case 1: bytes = *(int8_t *)address; break;
    case 2: bytes = *(int16_t *)address; break;
    case 4: bytes = *(int32_t *)address; break;
    default: {
        assert(0);
    } break;
    }

    lgdb_entry_value_t *entry = lgdb_get_from_table(&tinfo->type->uinfo.enum_type.value_to_name, bytes);

    char *enum_value_name = NULL;

    if (entry) {
        enum_value_name = (char *)*entry;
    }
    else {
        enum_value_name = "__UNKNOWN_ENUM__";
    }

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    if (modified) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        ImGui::TreeNodeEx(
            tinfo->sym_name,
            ImGuiTreeNodeFlags_Leaf |
            ImGuiTreeNodeFlags_Bullet |
            ImGuiTreeNodeFlags_NoTreePushOnOpen |
            ImGuiTreeNodeFlags_SpanFullWidth);
        ImGui::PopStyleColor();
    }
    else {
        ImGui::TreeNodeEx(
            tinfo->sym_name,
            ImGuiTreeNodeFlags_Leaf |
            ImGuiTreeNodeFlags_Bullet |
            ImGuiTreeNodeFlags_NoTreePushOnOpen |
            ImGuiTreeNodeFlags_SpanFullWidth);
    }
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%s (%d)", enum_value_name, bytes);
    ImGui::TableSetColumnIndex(2);
    ImGui::TextUnformatted(tinfo->type->name);
    ImGui::TableSetColumnIndex(3);
    ImGui::Text("%d", tinfo->size);

    return 0;
}
