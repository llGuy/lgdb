#include "debugger.hpp"


extern "C" {
#include <lgdb_context.h>
}


#include <TextEditor.h>
#include <stdlib.h>
#include <chrono>
#include <iostream>
#include <string>
#include <mutex>
#include <fstream>
#include <condition_variable>


char debugger_t::strdir_buffer[262] = { 0 };


static void s_debugger_loop_proc(shared_t *shared) {
    /* Timeout = 10 milliseconds*/
    uint32_t timeout = 3;

    bool active = true;
    shared->processing_events = false;

    while (active) {
        { // Check if received task from main thread (create process for example)
            std::lock_guard<std::mutex> lock (shared->ctx_mutex);
            for (uint32_t i = 0; i < shared->pending_task_count; ++i) {
                shared->tasks[i]->execute(shared);
                delete shared->tasks[i];
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
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
        }
    }
}


void debugger_t::init() {
    open_panels_.bits = 0;
    open_panels_.code = 1;
    open_panels_.watch = 1;
    open_panels_.dissassembly = 1;
    open_panels_.output = 1;

    editor_.SetText("");
    editor_.SetShowWhitespaces(0);
    editor_.SetColorizerEnable(0);
    editor_.SetReadOnly(1);

    output_buffer_max_ = lgdb_megabytes(1);
    output_buffer_ = new char[output_buffer_max_];
    memset(output_buffer_, 0, output_buffer_max_);
    output_buffer_counter_ = 0;

    shared_ = new shared_t;
    shared_->ctx = lgdb_create_context();
    shared_->pending_task_count = 0;

    loop_thread_ = std::thread(s_debugger_loop_proc, shared_);
}


void debugger_t::tick(ImGuiID main) {
    handle_debug_event();

    ImGui::SetNextWindowDockID(main, ImGuiCond_FirstUseEver);
    ImGui::Begin("Code");

    if (open_panels_.code) {
        editor_.Render("Code");
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

    ImGui::End();
}


void debugger_t::start_with_debugger() {
}


void debugger_t::start_without_debugger() {
}


void debugger_t::start_and_break_at_main() {
    /* Check for WinMain as well */
    std::lock_guard<std::mutex> lock(shared_->ctx_mutex);
    shared_->tasks[shared_->pending_task_count++] = new debugger_task_start_process_t();
}


void debugger_t::step_over() {
}


void debugger_t::step_in() {
}


void debugger_t::step_out() {
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

            copy_to_output_buffer("Hit a breakpoint at: ");
            copy_to_output_buffer(lvbh_data->file_name);

            char linenum[10] = {};
            sprintf(linenum, ":%d\n", lvbh_data->line_number);

            copy_to_output_buffer(linenum);

            std::ifstream is(lvbh_data->file_name);

            std::string file_contents;
            std::getline(is, file_contents, '\0');
            editor_.SetText(file_contents);

            TextEditor::Coordinates coords;
            coords.mColumn = 0;
            coords.mLine = lvbh_data->line_number - 1;
            editor_.SetCursorPosition(coords);
        } break;
        case LUET_SINGLE_ASM_STEP: {

        } break;
        case LUET_SOURCE_CODE_STEP_FINISHED: {

        } break;
        case LUET_STEP_OUT_FUNCTION_FINISHED: {

        } break;
        case LUET_STEP_IN_FUNCTION_FINISHED: {

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
