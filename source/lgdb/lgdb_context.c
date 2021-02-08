#define _CRT_SECURE_NO_WARNINGS

// MapViewOfFile
#include <stdio.h>
#include <Windows.h>
#include <DbgHelp.h>
#include <psapi.h>

#include "lgdb_context.h"
#include "lgdb_event.h"
#include "lgdb_utility.h"


lgdb_process_ctx_t *lgdb_create_context(const char *directory, const char *exe_name) {
    lgdb_process_ctx_t *ctx = (lgdb_process_ctx_t *)malloc(sizeof(lgdb_process_ctx_t));
    memset(ctx, 0, sizeof(lgdb_process_ctx_t));
    ctx->directory = directory;
    ctx->exe_name = exe_name;
    ctx->breakpoints.addr64_to_ud_idx = lgdb_create_table(LGDB_MAX_BREAKPOINTS, LGDB_MAX_BREAKPOINTS);
    ctx->call_stack.frame_count = 0;
    ZydisDecoderInit(&ctx->dissasm.decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_ADDRESS_WIDTH_64);
    ZydisFormatterInit(&ctx->dissasm.formatter, ZYDIS_FORMATTER_STYLE_INTEL);

    return ctx;
}


void lgdb_free_context(lgdb_process_ctx_t *ctx) {
    /* TODO: Make sure to free all the things inside ctx */
    free(ctx->exe_path);
    free(ctx);
}


bool32_t lgdb_begin_process(lgdb_process_ctx_t *ctx) {
    const char *exe_path = lgdb_concat_cstr(ctx->directory, ctx->exe_name);

    // Most of the fields here are not needed unless we set it in dwFlags
    STARTUPINFO startup_info = {
        .cb = sizeof(STARTUPINFO),
        .lpReserved = NULL,
        .lpDesktop = NULL,
        .lpTitle = ctx->exe_name,
        .dwX = 0, // Perhaps do something by taking the display rect
        .dwY = 0,
        .dwXSize = 0,
        .dwYSize = 0,
        .dwXCountChars = 0,
        .dwYCountChars = 0,
        .dwFlags = 0, // May need to look into this later
        .wShowWindow = 0,
        .cbReserved2 = 0,
        .lpReserved2 = NULL,
        .hStdInput = NULL,
        .hStdError = NULL
    };

    BOOL success = WIN32_CALL(CreateProcessA,
        exe_path,
        NULL, // If parameters are to be passed, write the cmdline command here
        NULL,
        NULL,
        FALSE,
        DEBUG_ONLY_THIS_PROCESS,
        NULL,
        ctx->directory,
        &startup_info,
        &ctx->proc_info);

    ctx->exe_path = exe_path;

    if (success) {
        ctx->is_running = 1;
    }

    return (bool32_t)success;
}


void lgdb_close_process(lgdb_process_ctx_t *ctx) {
    CloseHandle(ctx->proc_info.hThread);
    CloseHandle(ctx->proc_info.hProcess);
}


void lgdb_poll_debug_events(lgdb_process_ctx_t *ctx) {
    // TODO: Figure out what the difference between process and debug_ev.u.CreateProcessInfo.hProcess is

    /* Wait for debugger event to occur */
    // TODO: Replace INFINITE with 0 so that we can easily check for events in GUI app
    BOOL success = WaitForDebugEvent(&ctx->current_event, INFINITE);

    if (success) {
        switch (ctx->current_event.dwDebugEventCode) {

        case EXCEPTION_DEBUG_EVENT: {
            lgdb_handle_exception_debug_event(ctx);
        } break;

        case CREATE_PROCESS_DEBUG_EVENT: {
            lgdb_handle_create_process_debug_event(ctx);
        } break;

        case CREATE_THREAD_DEBUG_EVENT: {
            /*
                As needed, examine or change the thread's registers
                with the GetThreadContext and SetThreadContext functions;
                and suspend and resume thread execution with the
                SuspendThread and ResumeThread functions
            */
            lgdb_handle_create_thread_debug_event(ctx);
        } break;

        case EXIT_THREAD_DEBUG_EVENT: {
            lgdb_handle_exit_thread_debug_event(ctx);
        } break;

        case EXIT_PROCESS_DEBUG_EVENT: {
            lgdb_handle_exit_process_debug_event(ctx);
        } break;

        case LOAD_DLL_DEBUG_EVENT: {
            lgdb_handle_load_dll_debug_event(ctx);
        } break;

        case UNLOAD_DLL_DEBUG_EVENT: {
            lgdb_handle_unload_dll_debug_event(ctx);
        } break;

        case OUTPUT_DEBUG_STRING_EVENT: {
            lgdb_handle_output_debug_string_event(ctx);
        } break;

        }
    }
    else {
        printf("Didn't receive debug event\n");
    }
}


void lgdb_continue_process(lgdb_process_ctx_t *ctx) {
    lgdb_flush_pending_breakpoints(ctx);

    ContinueDebugEvent(
        ctx->current_event.dwProcessId,
        ctx->current_event.dwThreadId,
        DBG_CONTINUE);
}


BOOL lgdb_retrieve_thread_context(lgdb_process_ctx_t *ctx) {
    memset(&ctx->thread_ctx, 0, sizeof(CONTEXT));
    ctx->thread_ctx.ContextFlags = CONTEXT_FULL;

    return WIN32_CALL(
        GetThreadContext,
        ctx->proc_info.hThread,
        &ctx->thread_ctx);
}


void lgdb_sync_process_thread_context(lgdb_process_ctx_t *ctx) {
    WIN32_CALL(SetThreadContext, ctx->proc_info.hThread, &ctx->thread_ctx);
}
