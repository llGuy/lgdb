#define _CRT_SECURE_NO_WARNINGS

// MapViewOfFile
#include <stdio.h>
#include <Windows.h>
#include <DbgHelp.h>
#include <psapi.h>

#include "lgdb_context.h"
#include "lgdb_event.h"
#include "lgdb_utility.h"


lgdb_process_ctx_t *lgdb_create_context() {
    lgdb_process_ctx_t *ctx = (lgdb_process_ctx_t *)malloc(sizeof(lgdb_process_ctx_t));
    memset(ctx, 0, sizeof(lgdb_process_ctx_t));
    ctx->breakpoints.addr64_to_ud_idx = lgdb_create_table(LGDB_MAX_BREAKPOINTS, LGDB_MAX_BREAKPOINTS, NULL, NULL);
    ctx->call_stack.frame_count = 0;
    ctx->lnmem = lgdb_create_linear_allocator((uint32_t)lgdb_kilobytes(300));
    ctx->events = lgdb_create_linear_allocator((uint32_t)lgdb_kilobytes(5));
    ctx->symbols.sym_name_to_ptr = lgdb_create_table(LGDB_MAX_LOADED_DATA_SYMBOLS, LGDB_MAX_LOADED_DATA_SYMBOLS, NULL, NULL);
    ctx->symbols.type_idx_to_ptr = lgdb_create_table(LGDB_MAX_LOADED_DATA_SYMBOLS, LGDB_MAX_LOADED_DATA_SYMBOLS, NULL, NULL);
    ctx->symbols.data_mem = lgdb_create_linear_allocator((uint32_t)lgdb_kilobytes(300));
    ctx->symbols.type_mem = lgdb_create_linear_allocator((uint32_t)lgdb_kilobytes(300));
    ctx->symbols.copy_mem = lgdb_create_linear_allocator((uint32_t)lgdb_kilobytes(300));
    ctx->symbols.data_symbol_count = 0;
    ctx->symbols.symbol_ptr_pool = (lgdb_symbol_t **)malloc(sizeof(lgdb_symbol_t *) * LGDB_MAX_LOADED_DATA_SYMBOLS);
    ctx->block_on_wait = 0;

    ZydisDecoderInit(&ctx->dissasm.decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_ADDRESS_WIDTH_64);
    ZydisFormatterInit(&ctx->dissasm.formatter, ZYDIS_FORMATTER_STYLE_INTEL);

    return ctx;
}


void lgdb_open_process_context(lgdb_process_ctx_t *ctx, const char *directory, const char *exe_name) {
    ctx->directory = directory;
    ctx->exe_name = exe_name;
}


void lgdb_close_process_context(lgdb_process_ctx_t *ctx) {
    free((char *)ctx->exe_path);
    ctx->exe_path = NULL;
    ctx->directory = NULL;
    lgdb_clear_table(&ctx->breakpoints.addr64_to_ud_idx);
    ctx->call_stack.frame_count = 0;
    lgdb_lnclear(&ctx->lnmem);
    lgdb_clear_table(&ctx->symbols.sym_name_to_ptr);
    lgdb_clear_table(&ctx->symbols.type_idx_to_ptr);
    lgdb_lnclear(&ctx->symbols.data_mem);
    lgdb_lnclear(&ctx->symbols.type_mem);
    lgdb_lnclear(&ctx->symbols.copy_mem);
    ctx->symbols.data_symbol_count = 0;
}


void lgdb_free_context(lgdb_process_ctx_t *ctx) {
    /* TODO: Make sure to free all the things inside ctx */
    free((char *)ctx->exe_path);
    free(ctx);
}


bool32_t lgdb_begin_process(lgdb_process_ctx_t *ctx) {
    const char *exe_path = lgdb_concat_cstr(ctx->directory, ctx->exe_name);

    // Most of the fields here are not needed unless we set it in dwFlags
    STARTUPINFO startup_info = {
        .cb = sizeof(STARTUPINFO),
        .lpReserved = NULL,
        .lpDesktop = NULL,
        .lpTitle = (char *)ctx->exe_name,
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


bool32_t lgdb_terminate_process(lgdb_process_ctx_t *ctx) {
    UINT exit = 0;
    return WIN32_CALL(TerminateProcess, ctx->proc_info.hProcess, exit);
}


void lgdb_close_process(lgdb_process_ctx_t *ctx) {
    CloseHandle(ctx->proc_info.hThread);
    CloseHandle(ctx->proc_info.hProcess);
}


void lgdb_get_debug_event(lgdb_process_ctx_t *ctx, uint32_t timeout) {
    // TODO: Figure out what the difference between process and debug_ev.u.CreateProcessInfo.hProcess is

    /* Wait for debugger event to occur */
    // TODO: Replace INFINITE with 0 so that we can easily check for events in GUI app
    BOOL success = WaitForDebugEvent(&ctx->current_event, timeout);

    ctx->received_event = success;
}


bool32_t lgdb_translate_debug_event(lgdb_process_ctx_t *ctx) {
    if (ctx->received_event) {
        lgdb_retrieve_thread_context(ctx);

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

        return 1;
    }
    else {
        return 0;
    }
}


void lgdb_clear_events(lgdb_process_ctx_t *ctx) {
    lgdb_lnclear(&ctx->events);
    ctx->event_head = NULL;
    ctx->event_tail = NULL;
    ctx->triggered_user_event = 0;
    ctx->require_input = 0;
}


void lgdb_continue_process(lgdb_process_ctx_t *ctx) {
    lgdb_flush_pending_breakpoints(ctx);

    uint8_t op_byte;
    size_t bytes_read;

    WIN32_CALL(
        ReadProcessMemory,
        ctx->proc_info.hProcess,
        (void *)ctx->thread_ctx.Rip,
        &op_byte,
        1,
        &bytes_read);

    if (op_byte == 0xCC) {
        lgdb_entry_value_t *breakpoint_hdl = lgdb_get_from_tablep(
            &ctx->breakpoints.addr64_to_ud_idx,
            (void *)(ctx->thread_ctx.Rip));

        if (!breakpoint_hdl && ctx->thread_ctx.Rip != ctx->breakpoints.single_step_breakpoint.addr) {
            ++ctx->thread_ctx.Rip;
            lgdb_sync_process_thread_context(ctx);
        }
    }

    ContinueDebugEvent(
        ctx->current_event.dwProcessId,
        ctx->current_event.dwThreadId,
        DBG_CONTINUE);

    ctx->require_input = 0;

    lgdb_lnclear(&ctx->lnmem);
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


void lgdb_print_diagnostics(lgdb_process_ctx_t *ctx) {
    printf("###### DIAGNOSTICS ######\n");
    printf("--- MEMORY DIAGNOSTICS ---\n");
    lgdb_lndiagnostic_print(&ctx->lnmem, "Context Linear Allocator");
    lgdb_lndiagnostic_print(&ctx->symbols.data_mem, "Data Linear Allocator (used for allocating lgdb_symbol_t structures)");
    lgdb_lndiagnostic_print(&ctx->symbols.type_mem, "Type Linear Allocator (used for allocating anything to do with type info)");
    lgdb_lndiagnostic_print(&ctx->symbols.copy_mem, "Copy Linear Allocator (used for allocating copies of the bytes of structures from debuggee)");
    printf("--- SYMBOL DIAGNOSTICS ---\n");
    printf("");
}
