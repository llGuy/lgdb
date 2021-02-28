#include "debugger.hpp"


extern "C" {
#include <lgdb_context.h>
}

#include <TextEditor.h>


char debugger_t::strdir_buffer[262] = { 0 };


void debugger_t::init() {
    process_ctx_ = lgdb_create_context();
    open_panels_.bits = 0;
    open_panels_.code = 1;
    open_panels_.watch = 1;
    open_panels_.dissassembly = 1;
    open_panels_.output = 1;

    editor_.SetText("");
    editor_.SetShowWhitespaces(0);
    editor_.SetColorizerEnable(0);
    editor_.SetReadOnly(1);
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

    }

    ImGui::End();
}

void debugger_t::start_with_debugger() {
}

void debugger_t::start_without_debugger() {
}

void debugger_t::start_and_break_at_main() {
    /* Check for WinMain as well */
    lgdb_add_breakpointp(process_ctx_, "main");
    lgdb_begin_process(process_ctx_);
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
    lgdb_open_process_context(dbg->process_ctx_, strdir_buffer, strdir_buffer + directory_end + 2);

    OutputDebugStringA("Opened process\n");
}


void debugger_t::handle_debug_event() {
    lgdb_user_event_t ev = {};
    bool got_event = lgdb_get_debug_event(process_ctx_, &ev);

    if (got_event) {
        switch (ev.ev_type) {
        case LUET_EXCEPTION: {
        } break;
        case LUET_CREATE_PROCESS: {
            lgdb_continue_process(process_ctx_);
        } break;
        case LUET_EXIT_PROCESS: {
            lgdb_continue_process(process_ctx_);
        } break;
        case LUET_CREATE_THREAD: {
            lgdb_continue_process(process_ctx_);

        } break;
        case LUET_EXIT_THREAD: {
            lgdb_continue_process(process_ctx_);

        } break;
        case LUET_LOAD_DLL: {

            lgdb_continue_process(process_ctx_);
        } break;
        case LUET_UNLOAD_DLL: {
            lgdb_continue_process(process_ctx_);

        } break;
        case LUET_OUTPUT_DEBUG_STRING: {

            lgdb_continue_process(process_ctx_);
        } break;
        case LUET_VALID_BREAKPOINT_HIT: {
            printf("Hit a breakpoint\n");
        } break;
        case LUET_SINGLE_ASM_STEP: {

        } break;
        case LUET_SOURCE_CODE_STEP_FINISHED: {

        } break;
        case LUET_STEP_OUT_FUNCTION_FINISHED: {

        } break;
        case LUET_STEP_IN_FUNCTION_FINISHED: {

        } break;
        }
    }
}
