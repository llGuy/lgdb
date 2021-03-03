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
#include <condition_variable>
#include <imgui_internal.h>


char debugger_t::strdir_buffer[262] = { 0 };


void debugger_task_start_process(shared_t *shared) {
    lgdb_add_breakpointp(shared->ctx, "main");
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

    }

    if (open_panels_.dissassembly) {

    }

    if (open_panels_.output) {
        ImGui::Begin("Output", NULL, ImGuiWindowFlags_AlwaysVerticalScrollbar);
        ImGui::Text(output_buffer_);
        ImGui::End();
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


void debugger_t::continue_process() {
    std::lock_guard<std::mutex> lock(shared_->ctx_mutex);
    shared_->tasks[shared_->pending_task_count++] = debugger_task_continue;
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
            lgdb_continue_process(shared_->ctx);

            copy_to_output_buffer("Created process\n");
        } break;
        case LUET_EXIT_PROCESS: {
            lgdb_continue_process(shared_->ctx);
        } break;
        case LUET_CREATE_THREAD: {
            lgdb_continue_process(shared_->ctx);

        } break;
        case LUET_EXIT_THREAD: {
            lgdb_continue_process(shared_->ctx);
        } break;
        case LUET_LOAD_DLL: {
            lgdb_continue_process(shared_->ctx);
        } break;
        case LUET_UNLOAD_DLL: {
            lgdb_continue_process(shared_->ctx);
        } break;
        case LUET_OUTPUT_DEBUG_STRING: {
            lgdb_continue_process(shared_->ctx);
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
        } break;
        case LUET_SINGLE_ASM_STEP: {

        } break;
        case LUET_SOURCE_CODE_STEP_FINISHED: {
            auto *data = (lgdb_user_event_source_code_step_finished_t *)ev->ev_data;

            source_file_t *src_file = update_text_editor_file(data->file_name);

            TextEditor::Coordinates coords;
            coords.mColumn = 0;
            coords.mLine = data->line_number - 1;

            src_file->editor.SetCursorPosition(coords);
            src_file->editor.SetCurrentLineStepping(data->line_number - 1);
        } break;
        case LUET_STEP_OUT_FUNCTION_FINISHED: {
            auto *data = (lgdb_user_event_source_code_step_finished_t *)ev->ev_data;

            source_file_t *src_file = update_text_editor_file(data->file_name);

            TextEditor::Coordinates coords;
            coords.mColumn = 0;
            coords.mLine = data->line_number - 1;

            src_file->editor.SetCursorPosition(coords);
            src_file->editor.SetCurrentLineStepping(data->line_number - 1);
        } break;
        case LUET_STEP_IN_FUNCTION_FINISHED: {
            auto *data = (lgdb_user_event_step_in_finished_t *)ev->ev_data;

            source_file_t *src_file = update_text_editor_file(data->file_name);

            TextEditor::Coordinates coords;
            coords.mColumn = 0;
            coords.mLine = data->line_number - 1;

            src_file->editor.SetCursorPosition(coords);
            src_file->editor.SetCurrentLineStepping(data->line_number - 1);
        } break;

        default: {
            lgdb_continue_process(shared_->ctx);
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
