#define _NO_CVCONST_H
#include "debugger.hpp"


extern "C" {
#include <lgdb_context.h>
#include <lgdb_step.h>
}


#include <TextEditor.h>
#include <stdlib.h>
#include <chrono>
#include <iostream>
#include <string>
#include <mutex>
#include <fstream>
#include <DbgHelp.h>
#include <TypeInfoStructs.h>
#include <condition_variable>
#include <imgui_internal.h>


char debugger_t::strdir_buffer[262] = { 0 };


void debugger_task_start_process(shared_t *shared) {
    lgdb_add_breakpointp(shared->ctx, NULL); // Find entry point
    lgdb_begin_process(shared->ctx);
    shared->processing_events = 1;
}


void debugger_task_step_over(shared_t *shared) {
    lgdb_single_source_step(shared->ctx);
    lgdb_continue_process(shared->ctx);
    shared->processing_events = 1;
}


void debugger_task_step_into(shared_t *shared) {
    lgdb_step_into(shared->ctx);
    lgdb_continue_process(shared->ctx);
    shared->processing_events = 1;
}


void debugger_task_step_out(shared_t *shared) {
    lgdb_step_out(shared->ctx);
    lgdb_continue_process(shared->ctx);
    shared->processing_events = 1;
}


void debugger_task_continue(shared_t *shared) {
    lgdb_continue_process(shared->ctx);
    shared->processing_events = 1;
}


void debugger_task_toggle_breakpoint(shared_t *shared) {
    lgdb_add_breakpointfl(
        shared->ctx,
        shared->task_parameters.toggle_breakpoint.file_name,
        shared->task_parameters.toggle_breakpoint.line_number);
}


static void s_debugger_loop_proc(shared_t *shared) {
    /* Timeout = 10 milliseconds*/
    uint32_t timeout = 1;

    bool active = true;
    shared->processing_events = false;

    while (active) {
        { // Check if received task from main thread (create process for example)
            std::lock_guard<std::mutex> lock (shared->ctx_mutex);
            for (uint32_t i = 0; i < shared->pending_task_count; ++i) {
                (shared->tasks[i])(shared);
            }
            shared->pending_task_count = 0;
        }
        
        if (shared->processing_events) {
            lgdb_get_debug_event(shared->ctx, timeout);

            if (shared->ctx->received_event) {
                /* Lock */
                std::lock_guard<std::mutex> lock (shared->ctx_mutex);

                lgdb_user_event_t ev = {};
                bool got_event = lgdb_translate_debug_event(shared->ctx);

                if (got_event) {
                    if (shared->ctx->require_input) {
                        shared->processing_events = false;
                    }
                    else {
                        lgdb_continue_process(shared->ctx);
                    }
                }
            }
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}


void debugger_t::init() {
    open_panels_.bits = 0;
    open_panels_.code = 1;
    open_panels_.watch = 1;
    open_panels_.call_stack = 1;
    open_panels_.dissassembly = 1;
    open_panels_.output = 1;

    source_files_.reserve(50);

    output_buffer_max_ = lgdb_megabytes(1);
    output_buffer_ = new char[output_buffer_max_];
    memset(output_buffer_, 0, output_buffer_max_);
    output_buffer_counter_ = 0;

    shared_ = new shared_t;
    shared_->ctx = lgdb_create_context();
    shared_->pending_task_count = 0;

    loop_thread_ = std::thread(s_debugger_loop_proc, shared_);

    is_running_ = 0;

    current_src_file_idx_ = -1;

    symbol_ptr_count_ = 0;
    max_symbol_ptr_count_ = 500;
    symbol_ptr_pool_ = new variable_info_t *[max_symbol_ptr_count_];

    watch_frames_.reserve(30);

    current_stack_frame_ = 0xFFFFFFFFFFFFFFFF;
    current_watch_frame_idx_ = -1;

    variable_info_allocator_ = lgdb_create_linear_allocator(lgdb_megabytes(1));
    variable_copy_allocator_ = lgdb_create_linear_allocator(lgdb_megabytes(1));

    is_process_suspended_ = 0;
    changed_frame_ = 0;
}


void debugger_t::tick(ImGuiID main) {
    handle_debug_event();

    ImGui::SetNextWindowDockID(main, ImGuiCond_FirstUseEver);

    if (current_src_file_idx_ >= 0) {
        for (uint32_t i = 0; i < source_files_.size(); ++i) {
            source_file_t *src = source_files_[i];

            if (ImGui::Begin(src->file_name.c_str())) {
                src->editor.Render("Source");
            }

            ImGui::End();
        }
    }
    else {
        ImGui::Begin("Code", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration);
        ImGui::End();
    }

    if (open_panels_.watch) {
        ImGui::Begin("Watch");

        if (ImGui::BeginTable("Watch", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {

            ImGui::TableSetupColumn("Name", 0, 0.3f);
            ImGui::TableSetupColumn("Value", 0, 0.3f);
            ImGui::TableSetupColumn("Type", 0, 0.3f);
            ImGui::TableSetupColumn("Size", 0, 0.1f);
            ImGui::TableHeadersRow();

            if (is_process_suspended_) {
                uint32_t call_stack_idx = 0;
                for (int32_t i = watch_frames_.size() - 1; i >= 0; --i) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);

                    watch_frame_t *watch_frame = &watch_frames_[i];

                    bool opened;
                    if (watch_frame->flags.is_stack_frame) {
                        if (changed_frame_) {
                            if (i == current_watch_frame_idx_) {
                                ImGui::SetNextTreeNodeOpen(true);
                            }
                            else {
                                ImGui::SetNextTreeNodeOpen(false);
                            }
                        }

                        opened = ImGui::TreeNodeEx(call_stack_[call_stack_idx].function_name.c_str(), ImGuiTreeNodeFlags_SpanFullWidth);

                        ++call_stack_idx;
                    }
                    else {
                        opened = ImGui::TreeNodeEx("Some watch", ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen);
                    }

                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextDisabled("{...}");
                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextUnformatted("--");
                    ImGui::TableSetColumnIndex(3);
                    ImGui::TextUnformatted("--");

                    if (opened) {

                        for (uint32_t i = 0; i < watch_frame->var_count; i++) {
                            variable_info_t *var = watch_frame->symbol_ptr_pool_start[i];

                            render_symbol_type_data(
                                var,
                                var->sym.name,
                                NULL,
                                var->sym.debugger_bytes_ptr,
                                var->sym.size,
                                lgdb_get_type(shared_->ctx, var->sym.type_index));
                        }

                        ImGui::TreePop();
                    }
                }
            }

            ImGui::EndTable();

            changed_frame_ = false;
        }

        ImGui::End();
    }

    if (open_panels_.dissassembly) {

    }

    if (open_panels_.output) {
        ImGui::Begin("Output", NULL, ImGuiWindowFlags_AlwaysVerticalScrollbar);
        ImGui::Text(output_buffer_);
        ImGui::End();
    }

    if (open_panels_.call_stack) {
        ImGui::Begin("Call Stack");

        if (ImGui::BeginTable("Watch", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {

            ImGui::TableSetupColumn("Address", 0.1f);
            ImGui::TableSetupColumn("Function", 0.25f);
            ImGui::TableSetupColumn("Module", 0.3f);
            ImGui::TableSetupColumn("File Name", 0.3f);
            ImGui::TableSetupColumn("Line Number", 0.05f);
            ImGui::TableHeadersRow();

            for (auto frame : call_stack_) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%p", (void *)frame.addr);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text(frame.function_name.c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::Text(frame.module_name.c_str());
                ImGui::TableSetColumnIndex(3);
                ImGui::Text(frame.file_name.c_str());
                ImGui::TableSetColumnIndex(4);
                ImGui::Text("%d", frame.line_number);
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("[Mystical External Code]");
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("--");
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("--");
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("--");
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("--");

            ImGui::EndTable();

        }
        
        ImGui::End();
    }
}


void debugger_t::render_symbol_base_type_data(const char *name, const char *type_name, void *address, uint32_t size, lgdb_symbol_type_t *type) {
    const char *type_name_actual = type_name;
    if (!type_name)
        type_name_actual = lgdb_get_base_type_string(type->uinfo.base_type.base_type);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);

    uint32_t count = size / type->size;

    if (count == 1) {
        ImGui::TreeNodeEx(name, ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth);
        ImGui::TableSetColumnIndex(1);

        if (size / type->size == 1) {
            switch (type->uinfo.base_type.base_type) {
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
        ImGui::Text("%d", size);
    }
    else {
        assert(0);
    }
}


bool debugger_t::render_composed_var_row(const char *name, const char *type, uint32_t size) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    bool opened_struct = ImGui::TreeNodeEx(name, ImGuiTreeNodeFlags_SpanFullWidth);

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


void debugger_t::render_symbol_type_data(
    variable_info_t *var,
    const char *sym_name,
    const char *type_name,
    void *address,
    uint32_t size,
    lgdb_symbol_type_t *type) {
    switch (type->tag) {
    case SymTagBaseType: render_symbol_base_type_data(
        sym_name,
        type_name,
        address,
        size,
        type); break;

    case SymTagTypedef: {
        lgdb_symbol_type_t *typedefed_type = lgdb_get_type(shared_->ctx, type->uinfo.typedef_type.type_index);
        render_symbol_type_data(var, sym_name, type->name, address, size, typedefed_type);

        break;
    };

    case SymTagUDT: {
        bool opened = render_composed_var_row(sym_name, type->name, size);

        if (opened) {
            for (uint32_t i = 0; i < type->uinfo.udt_type.base_classes_count; ++i) {
                lgdb_base_class_t *base_class = &type->uinfo.udt_type.base_classes_type_idx[i];
                lgdb_symbol_type_t *base_class_type = lgdb_get_type(shared_->ctx, base_class->type_idx);
                render_symbol_type_data(var, base_class_type->name, base_class_type->name, (uint8_t *)address + base_class->offset, base_class->size, base_class_type);
            }

            for (uint32_t i = 0; i < type->uinfo.udt_type.member_var_count; ++i) {
                lgdb_member_var_t *member_var = &type->uinfo.udt_type.member_vars_type_idx[i];
                lgdb_symbol_type_t *member_type = lgdb_get_type(shared_->ctx, member_var->type_idx);
                render_symbol_type_data(var, member_var->name, member_type->name, (uint8_t *)address + member_var->offset, member_var->size, member_type);
            }

            ImGui::TreePop();
        }

        break;
    };

    case SymTagArrayType: {
        lgdb_symbol_type_t *arrayed_type = lgdb_get_type(shared_->ctx, type->uinfo.array_type.type_index);

        uint32_t element_count = size / arrayed_type->size;

        const char *arrayed_type_name = arrayed_type->name;
        if (arrayed_type->tag == SymTagBaseType) {
            arrayed_type_name = lgdb_get_base_type_string(arrayed_type->uinfo.base_type.base_type);
        }

        char buffer[50] = {};
        sprintf(buffer, "%s[%d]", arrayed_type_name, element_count);

        bool opened = render_composed_var_row(sym_name, buffer, size);

        if (opened) {
            for (uint32_t i = 0; i < element_count && i < 30; ++i) {
                sprintf(buffer, "[%d]", i);
                render_symbol_type_data(var, buffer, arrayed_type->name, (uint8_t *)address + arrayed_type->size * i, arrayed_type->size, arrayed_type);
            }

            ImGui::TreePop();
        }

        break;
    };

    case SymTagEnum: {
        lgdb_symbol_type_t *enum_type = lgdb_get_type(shared_->ctx, type->uinfo.enum_type.type_index);

        int32_t bytes;

        switch (size) {
        case 1: bytes = *(int8_t *)address; break;
        case 2: bytes = *(int16_t *)address; break;
        case 4: bytes = *(int32_t *)address; break;
        default: {
            assert(0);
        } break;
        }

        lgdb_entry_value_t *entry = lgdb_get_from_table(&type->uinfo.enum_type.value_to_name, bytes);

        char *enum_value_name = NULL;

        if (entry) {
            enum_value_name = (char *)*entry;
        }
        else {
            enum_value_name = "__UNKNOWN_ENUM__";
        }

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TreeNodeEx(sym_name, ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth);
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%s (%d)", enum_value_name, bytes);
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(type->name);
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%d", size);

        break;
    };

    case SymTagPointerType: {
        lgdb_symbol_type_t *pointed_type = lgdb_get_type(shared_->ctx, type->uinfo.array_type.type_index);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        bool opened = ImGui::TreeNodeEx(sym_name, ImGuiTreeNodeFlags_SpanFullWidth);

        if (!var->open && opened) {
            var->open = 1;
            var->requested = 1;
            deep_update_variable_info(shared_->ctx, var);
        }

        var->open = opened;
        
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%p", *(void **)address);
        ImGui::TableSetColumnIndex(2);

        const char *pointed_type_name = pointed_type->name;
        if (type->tag == SymTagBaseType) {
            pointed_type_name = lgdb_get_base_type_string(pointed_type->uinfo.base_type.base_type);
        }

        ImGui::Text("%s*", pointed_type_name);
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%d", size);

        if (opened) {
            variable_info_t *sub = var->sub_start;
            uint32_t count_in_buffer = sub->count_in_buffer;
            char name_buf[15] = {};
            for (uint32_t i = 0; i < count_in_buffer; i++) {
                lgdb_symbol_type_t *type = lgdb_get_type(shared_->ctx, sub->sym.type_index);
                sprintf(name_buf, "[%d]", i);
                render_symbol_type_data(sub, name_buf, type->name, sub->sym.debugger_bytes_ptr, type->size, type);
                sub++;
            }

            ImGui::TreePop();
        }
    };

    default: {};
    }
}


void debugger_t::start_with_debugger() {
}


void debugger_t::start_without_debugger() {
}


void debugger_t::start_and_break_at_main() {
    /* Check for WinMain as well */
    std::lock_guard<std::mutex> lock(shared_->ctx_mutex);
    shared_->tasks[shared_->pending_task_count++] = debugger_task_start_process;

    is_running_ = true;
}


void debugger_t::step_over() {
    std::lock_guard<std::mutex> lock(shared_->ctx_mutex);
    shared_->tasks[shared_->pending_task_count++] = debugger_task_step_over;
}


void debugger_t::step_into() {
    std::lock_guard<std::mutex> lock(shared_->ctx_mutex);
    shared_->tasks[shared_->pending_task_count++] = debugger_task_step_into;
}


void debugger_t::step_out() {
    std::lock_guard<std::mutex> lock(shared_->ctx_mutex);
    shared_->tasks[shared_->pending_task_count++] = debugger_task_step_out;
}


void debugger_t::toggle_breakpoint(const char *filename, int line) {
    if (current_src_file_idx_ >= 0) {
        std::lock_guard<std::mutex> lock(shared_->ctx_mutex);
        shared_->tasks[shared_->pending_task_count++] = debugger_task_toggle_breakpoint;

        // Add it in the text editor breakpoint list
        source_file_t *src_file = source_files_[current_src_file_idx_];
        auto coords = src_file->editor.GetCursorPosition();

        shared_->task_parameters.toggle_breakpoint.file_name = src_file->file_name.c_str();
        shared_->task_parameters.toggle_breakpoint.line_number = coords.mLine + 1;

        TextEditor::Breakpoints &breakpoints = src_file->editor.GetBreakpoints();
        
        auto it = breakpoints.find(coords.mLine + 1);
        if (it == breakpoints.end()) {
            breakpoints.insert(coords.mLine + 1);
        }
    }
}


void debugger_t::continue_process() {
    std::lock_guard<std::mutex> lock(shared_->ctx_mutex);
    shared_->tasks[shared_->pending_task_count++] = debugger_task_continue;

    if (current_src_file_idx_ >= 0) {
        source_file_t *src_file = source_files_[current_src_file_idx_];
        src_file->editor.SetCurrentLineStepping(-1);
    }
}


static const char *remove_path(const char *path) {
    size_t path_length = strlen(path);
    uint32_t directory_end = (uint32_t)(path_length - 1);
    for (; directory_end > 0; --directory_end) {
        if (path[directory_end] == '/' || path[directory_end] == '\\') {
            break;
        }
    }

    return &path[directory_end + 1];
}


/* Static */
void debugger_t::open_file_proc(const char *filename, void *obj) {
    debugger_t *dbg = (debugger_t *)obj;

    memset(strdir_buffer, 0, sizeof(strdir_buffer));

    size_t path_length = strlen(filename);
    uint32_t directory_end = (uint32_t)(path_length - 1);
    for (; directory_end > 0; --directory_end) {
        if (filename[directory_end] == '/' || filename[directory_end] == '\\') {
            break;
        }
    }

    memcpy(strdir_buffer, filename, directory_end + 1);
    memcpy(strdir_buffer + directory_end + 2, filename + directory_end + 1, path_length - directory_end);

    /* Initialise the context */
    std::lock_guard<std::mutex> lock (dbg->shared_->ctx_mutex);
    lgdb_open_process_context(dbg->shared_->ctx, strdir_buffer, strdir_buffer + directory_end + 2);

    OutputDebugStringA("Opened process\n");
}


source_file_t *debugger_t::update_text_editor_file(const char *file_name) {
    uint32_t path_hash = lgdb_hash_string(file_name);

    if (path_hash == current_src_file_hash_) {
        uint32_t idx = source_file_map_.at(path_hash);
        return source_files_[idx];
    }
    else {
        restore_current_file();

        auto it = source_file_map_.find(path_hash);
        if (it == source_file_map_.end()) {
            uint32_t path_len = strlen(file_name);

            const char *file_name_start = file_name;

            for (int32_t i = path_len - 1; i >= 0; --i) {
                if (file_name[i] == '\\' || file_name[i] == '/') {
                    file_name_start = &file_name[i] + 1;
                    break;
                }
            }

            /* Register new file */
            source_file_t *src = new source_file_t{ path_hash, std::ifstream(file_name) };
            src->file_name = std::string(file_name_start);

            source_files_.push_back(src);
            uint32_t idx = source_files_.size() - 1;
            source_file_map_[path_hash] = idx;

            std::string contents;

            std::getline(src->stream, contents, '\0');

            src->editor.SetShowWhitespaces(0);
            src->editor.SetColorizerEnable(0);
            src->editor.SetReadOnly(1);
            src->editor.SetText(contents);

            ImGui::DockBuilderDockWindow(src->file_name.c_str(), dock);

            current_src_file_idx_ = idx;
            current_src_file_hash_ = path_hash;

            return src;
        }
        else {
            uint32_t idx = source_file_map_[path_hash];
            current_src_file_idx_ = idx;
            current_src_file_hash_ = path_hash;

            source_file_t *src = source_files_[idx];

            ImGui::SetWindowFocus(src->file_name.c_str());

            return source_files_[idx];
        }
    }
}


void debugger_t::handle_debug_event() {
    std::lock_guard<std::mutex> lock(shared_->ctx_mutex);
    for (lgdb_user_event_t *ev = shared_->ctx->event_tail; ev; ev = ev->next) {
        switch (ev->ev_type) {
        case LUET_EXCEPTION: {
        } break;
        case LUET_CREATE_PROCESS: {
            // shared_->tasks[shared_->pending_task_count++] = debugger_task_continue;

            copy_to_output_buffer("Created process\n");
            is_process_suspended_ = 0;
        } break;
        case LUET_EXIT_PROCESS: {
            // shared_->tasks[shared_->pending_task_count++] = debugger_task_continue;
            is_process_suspended_ = 0;
        } break;
        case LUET_CREATE_THREAD: {
            // shared_->tasks[shared_->pending_task_count++] = debugger_task_continue;
            is_process_suspended_ = 0;
        } break;
        case LUET_EXIT_THREAD: {
            // shared_->tasks[shared_->pending_task_count++] = debugger_task_continue;
            is_process_suspended_ = 0;
        } break;
        case LUET_LOAD_DLL: {
#if 0
            auto *data = (lgdb_user_event_load_dll_t *)ev->ev_data;

            copy_to_output_buffer("Loaded DLL \"");
            copy_to_output_buffer(data->path);
            copy_to_output_buffer("\", ");

            if (data->symbols) {
                copy_to_output_buffer(" FOUND symbols\n");
            }
            else {
                copy_to_output_buffer(" DID NOT FIND symbols\n");
            }

            free((void *)data->path);
#endif

            // shared_->tasks[shared_->pending_task_count++] = debugger_task_continue;
            is_process_suspended_ = 0;
        } break;
        case LUET_UNLOAD_DLL: {
            // shared_->tasks[shared_->pending_task_count++] = debugger_task_continue;
            is_process_suspended_ = 0;
        } break;
        case LUET_OUTPUT_DEBUG_STRING: {
            // shared_->tasks[shared_->pending_task_count++] = debugger_task_continue;
            is_process_suspended_ = 0;
        } break;
        case LUET_VALID_BREAKPOINT_HIT: {
            /* Once a breakpoint was hit, don't block on wait */
            auto *lvbh_data = (lgdb_user_event_valid_breakpoint_hit_t *)ev->ev_data;

            source_file_t *src_file = update_text_editor_file(lvbh_data->file_name);

            copy_to_output_buffer("Hit a breakpoint at: ");
            copy_to_output_buffer(lvbh_data->file_name);

            char linenum[10] = {};
            sprintf(linenum, ":%d\n", lvbh_data->line_number);

            copy_to_output_buffer(linenum);

            TextEditor::Coordinates coords;
            coords.mColumn = 0;
            coords.mLine = lvbh_data->line_number - 1;

            src_file->editor.SetCursorPosition(coords);
            src_file->editor.SetCurrentLineStepping(lvbh_data->line_number - 1);

            update_locals(shared_->ctx);
            update_call_stack();

            is_process_suspended_ = 1;
        } break;
        case LUET_SINGLE_ASM_STEP: {

            is_process_suspended_ = 1;
        } break;
        case LUET_SOURCE_CODE_STEP_FINISHED: {
            auto *data = (lgdb_user_event_source_code_step_finished_t *)ev->ev_data;

            source_file_t *src_file = update_text_editor_file(data->file_name);

            TextEditor::Coordinates coords;
            coords.mColumn = 0;
            coords.mLine = data->line_number - 1;

            src_file->editor.SetCursorPosition(coords);
            src_file->editor.SetCurrentLineStepping(data->line_number - 1);

            update_locals(shared_->ctx);
            update_call_stack();

            is_process_suspended_ = 1;
        } break;
        case LUET_STEP_OUT_FUNCTION_FINISHED: {
            auto *data = (lgdb_user_event_source_code_step_finished_t *)ev->ev_data;

            source_file_t *src_file = update_text_editor_file(data->file_name);

            TextEditor::Coordinates coords;
            coords.mColumn = 0;
            coords.mLine = data->line_number - 1;

            src_file->editor.SetCursorPosition(coords);
            src_file->editor.SetCurrentLineStepping(data->line_number - 1);

            update_locals(shared_->ctx);
            update_call_stack();

            is_process_suspended_ = 1;
        } break;
        case LUET_STEP_IN_FUNCTION_FINISHED: {
            auto *data = (lgdb_user_event_step_in_finished_t *)ev->ev_data;

            source_file_t *src_file = update_text_editor_file(data->file_name);

            TextEditor::Coordinates coords;
            coords.mColumn = 0;
            coords.mLine = data->line_number - 1;

            src_file->editor.SetCursorPosition(coords);
            src_file->editor.SetCurrentLineStepping(data->line_number - 1);

            update_locals(shared_->ctx);
            update_call_stack();

            is_process_suspended_ = 1;
        } break;

        default: {
            is_process_suspended_ = 0;
            // shared_->tasks[shared_->pending_task_count++] = debugger_task_continue;
        } break;
        }
    }

    lgdb_clear_events(shared_->ctx);
}


void debugger_t::copy_to_output_buffer(const char *buf) {
    uint32_t buf_len = strlen(buf);
    strcpy_s(output_buffer_ + output_buffer_counter_, output_buffer_max_ - output_buffer_counter_, buf);
    output_buffer_counter_ += buf_len;
}


bool debugger_t::is_process_running() {
    return is_running_;
}


void debugger_t::restore_current_file() {
    if (current_src_file_idx_ >= 0) {
        source_files_[current_src_file_idx_]->editor.SetCurrentLineStepping(-1);
    }
}


/* Static */
void debugger_t::update_local_symbol(
    lgdb_process_ctx_t *ctx,
    const char *name,
    lgdb_symbol_t *sym,
    void *obj) {
    debugger_t *dbg = (debugger_t *)obj;

    watch_frame_t *frame = &dbg->watch_frames_[dbg->current_watch_frame_idx_];

    auto entry = dbg->sym_idx_to_ptr.find(sym->sym_index);

    variable_info_t *var = NULL;
    
    if (entry == dbg->sym_idx_to_ptr.end()) {
        // We need to allocate in variable info, etc...
        var = (variable_info_t *)lgdb_lnmalloc(
            &dbg->variable_info_allocator_,
            sizeof(variable_info_t));

        var->sym = *sym;
        var->open = 0;

        uint32_t name_len = strlen(name);
        var->sym.name = (char *)lgdb_lnmalloc(&dbg->variable_info_allocator_, (1 + name_len) * sizeof(char));
        memcpy(var->sym.name, name, name_len * sizeof(char));
        var->sym.name[name_len] = 0;

        /* Registers the type if it hasn't already been registered */
        lgdb_symbol_type_t *type = lgdb_get_type(ctx, var->sym.type_index);

        void *data = lgdb_lnmalloc(&dbg->variable_copy_allocator_, var->sym.size);
        var->sym.debugger_bytes_ptr = data;
        var->sym.user_flags = 0;

        var->sub_start = NULL;

        /* We need to store settings for each element / member (whether to collapse, etc...) */
        switch (type->tag) {

        default: {
            lgdb_read_buffer_from_process(
                ctx,
                (uint64_t)lgdb_get_real_symbol_address(ctx, &var->sym),
                var->sym.size,
                var->sym.debugger_bytes_ptr);
        } break;

        }

        dbg->sym_idx_to_ptr.insert(std::make_pair(sym->sym_index, var));
    }
    else {
        // This variabe is already registered
        var = entry->second;

        auto *type = lgdb_get_type(ctx, var->sym.type_index);

        switch (type->tag) {
        case SymTagPointerType: {
#if 0
            lgdb_read_buffer_from_process(
                ctx,
                (uint64_t)lgdb_get_real_symbol_address(ctx, &var->sym),
                var->sym.size,
                var->sym.debugger_bytes_ptr);

            if (var->open) {
                auto *pointed_type = lgdb_get_type(ctx, type->uinfo.pointer_type.type_index);
                uint64_t pointer_value = *(uint64_t *)var->sym.debugger_bytes_ptr;

                if (!var->sub_start) {
                    var->inspecting_count = var->requested;

                    /* Need to allocate space for inspecting count number of variables */
                    var->sub_start = (variable_info_t *)lgdb_lnmalloc(&dbg->variable_info_allocator_, sizeof(variable_info_t) * var->inspecting_count);
                    memset(var->sub_start, 0, sizeof(variable_info_t) * var->inspecting_count);

                    { // Initialise the linked list
                        auto *sub = var->sub_start;
                        for (uint32_t i = 0; i < var->inspecting_count - 1; ++i) {
                            sub->sym.real_addr = pointer_value + i * pointed_type->size;
                            sub->sym.type_index = pointed_type->index;
                            sub->sym.type_index = pointed_type->index;

                            sub->next = sub + 1;
                            sub = sub->next;
                        }

                        var->sub_end = sub;
                        sub->next = NULL;
                    }

                    if (pointed_type->tag != SymTagUDT && pointed_type->tag != SymTagArrayType) {
                        
                    }
                }
                else if (var->requested > var->inspecting_count) {
                    /* Add some more */
                    uint32_t diff = var->requested - var->inspecting_count;
                    var->sub_end->next = (variable_info_t *)lgdb_lnmalloc(&dbg->variable_info_allocator_, sizeof(variable_info_t) * diff);
                    memset(var->sub_end->next, 0, sizeof(variable_info_t) * var->inspecting_count);

                    { // Initialise the linked list from here
                        auto *sub = var->sub_end->next;
                        for (uint32_t i = 0; i < diff - 1; ++i) {
                            sub->sym.real_addr = pointer_value + (i + var->inspecting_count) * pointed_type->size;
                            sub->sym.type_index = pointed_type->index;
                            sub->next = sub + 1;
                            sub = sub->next;
                        }

                        var->sub_end = sub;
                        sub->next = NULL;
                    }

                    var->inspecting_count = var->requested;
                }

                // With everything allocated, read from the memory of the variable
                for (
                    variable_info_t *current_open = var->sub_start;
                    current_open;
                    current_open = current_open->next_open) {
                    
                }
            }
#endif
        } break;

        case SymTagArrayType: {
            
        } break;

        case SymTagUDT: {
            
        } break;

        default: {
            
        } break;
        }

        lgdb_read_buffer_from_process(
            ctx,
            (uint64_t)lgdb_get_real_symbol_address(ctx, &var->sym),
            var->sym.size,
            var->sym.debugger_bytes_ptr);
    }

    frame->symbol_ptr_pool_start[frame->var_count++] = var;
}


/* Static */
void debugger_t::update_call_stack(lgdb_process_ctx_t *ctx, void *obj, lgdb_call_stack_frame_t *frame) {
    debugger_t *dbg = (debugger_t *)obj;
    call_stack_frame_t frame_copy = {};
    frame_copy.addr = frame->addr;
    frame_copy.file_name = remove_path(frame->file_name);
    frame_copy.function_name = frame->function_name;
    frame_copy.line_number = frame->line_number;
    frame_copy.module_name = remove_path(frame->module_name);
    dbg->call_stack_.push_back(frame_copy);
}


void debugger_t::update_locals(lgdb_process_ctx_t *ctx) {
    uint64_t new_frame = lgdb_update_symbol_context(ctx);
    watch_frame_t *watch_frame = NULL;

    if (new_frame < current_stack_frame_) {
        /* Remove all the variables from the unoredered map from the previous scope */

        // We just called a function - push a new watch group
        watch_frames_.push_back(watch_frame_t{});
        uint32_t new_watch_idx = watch_frames_.size() - 1;

        watch_frame = &watch_frames_[new_watch_idx];
        watch_frame->stack_frame = new_frame;
        watch_frame->flags.is_stack_frame = 1;

        if (watch_frames_.size() == 1) {
            watch_frame->symbol_ptr_pool_start = symbol_ptr_pool_;
        }
        else {
            /* Get previous frame and set the symbol_ptr_pool_start to that one + var_count */
            watch_frame_t *previous_frame = &watch_frames_[current_watch_frame_idx_];
            watch_frame->symbol_ptr_pool_start = previous_frame->symbol_ptr_pool_start + previous_frame->var_count;
        }

        current_watch_frame_idx_ = watch_frames_.size() - 1;

        watch_frame->start_in_variable_info_allocator = variable_info_allocator_.current;

        current_stack_frame_ = new_frame;

        creating_new_frame_ = true;
        changed_frame_ = true;
    }
    else if (new_frame > current_stack_frame_) {
        /* Remove all the variables from the unoredered map from the previous scope */
        watch_frame_t *previous_frame = &watch_frames_[current_watch_frame_idx_];
        variable_info_allocator_.current = previous_frame->start_in_variable_info_allocator;

        watch_frames_.pop_back();

        // We just exited out of a function
        current_stack_frame_ = new_frame;

        if (watch_frames_.size())
            current_watch_frame_idx_ = watch_frames_.size() - 1;

        watch_frame = &watch_frames_[current_watch_frame_idx_];
        changed_frame_ = true;
    }
    else {
        watch_frame = &watch_frames_[current_watch_frame_idx_];
        changed_frame_ = false;
    }

    watch_frame->var_count = 0;
    lgdb_update_local_symbols(ctx, update_local_symbol, this);
}


void debugger_t::update_call_stack() {
    call_stack_.clear();
    lgdb_update_call_stack(shared_->ctx, this, debugger_t::update_call_stack);
}


void debugger_t::deep_update_variable_info(lgdb_process_ctx_t *ctx, variable_info_t *var) {
    lgdb_symbol_t *sym = &var->sym;
    lgdb_symbol_type_t *type = lgdb_get_type(ctx, sym->type_index);

    switch (type->tag) {
    case SymTagPointerType: {
        // Priority 1: read the value of the pointer itself
        lgdb_read_buffer_from_process(
            ctx,
            (uint64_t)lgdb_get_real_symbol_address(ctx, sym),
            sym->size,
            sym->debugger_bytes_ptr);

        // Priority 2: read the array of values that are being pointed
        if (var->open) {
            lgdb_symbol_type_t *pointed_type = lgdb_get_type(ctx, type->uinfo.pointer_type.type_index);
            uint64_t pointer_value = *(uint64_t *)sym->debugger_bytes_ptr;

            if (!var->sub_start) {
                var->inspecting_count = var->requested;

                var->sub_start = (variable_info_t *)lgdb_lnmalloc(
                    &variable_info_allocator_,
                    sizeof(variable_info_t) * var->inspecting_count);

                memset(var->sub_start, 0, sizeof(variable_info_t) * var->inspecting_count);

                { // Initialise the linked list
                    auto *sub = var->sub_start;
                    sub->count_in_buffer = var->requested;
                    sub->sym.real_addr = pointer_value;
                    sub->sym.type_index = pointed_type->index;

                    for (uint32_t i = 1; i < var->inspecting_count - 1; ++i) {
                        sub->sym.real_addr = pointer_value + i * pointed_type->size;
                        sub->sym.type_index = pointed_type->index;
                        sub->count_in_buffer = -1;

                        sub->next = sub + 1;
                        sub = sub->next;
                    }

                    var->sub_end = sub;
                    sub->next = NULL;
                }

                /* Only read in all the values if this isn't a composed type */
                if (pointed_type->tag != SymTagUDT && pointed_type->tag != SymTagArrayType) {
                    void *copy_buffer = lgdb_lnmalloc(
                        &variable_copy_allocator_,
                        pointed_type->size * var->requested);

                    lgdb_read_buffer_from_process(
                        ctx,
                        pointer_value,
                        pointed_type->size * var->requested,
                        copy_buffer);

                    auto *sub = var->sub_start;
                    for (uint32_t i = 0; i < var->inspecting_count; ++i) {
                        sub[i].sym.debugger_bytes_ptr = (uint8_t *)copy_buffer + pointed_type->size * i;
                    }
                }
            }
            else {
                // Go through each buffer
            }
        }
    } break;
    }
}
