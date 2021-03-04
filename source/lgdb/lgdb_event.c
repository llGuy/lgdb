#include <Windows.h>
#include <DbgHelp.h>
#include <stdio.h>
#include <assert.h>
#include "lgdb_step.h"
#include "lgdb_event.h"
#include "lgdb_symbol.h"
#include "lgdb_utility.h"
#include "lgdb_context.h"
#include "lgdb_exception.h"


void lgdb_handle_exception_debug_event(struct lgdb_process_ctx *ctx) {
    /*
        Process the exception code. When handling exceptions,
        remember to set the continuation status parameter
        This value is used by the ContinueDebugEvent function
    */
    switch (ctx->current_event.u.Exception.ExceptionRecord.ExceptionCode) {
    case EXCEPTION_ACCESS_VIOLATION: {
        /*
            First chance: pass this on to the system
            Last chance: display an appropriate error
        */
    } break;

    case EXCEPTION_BREAKPOINT: {
        lgdb_handle_breakpoint_exception(ctx);
    } break;

    case EXCEPTION_DATATYPE_MISALIGNMENT: {
        /*
            First chance: pass this on to the system
            Last chance: display an appropriate error
        */
    } break;

    case EXCEPTION_SINGLE_STEP: {
        lgdb_handle_single_step_exception(ctx);
    } break;

    case DBG_CONTROL_C: {
        /*
            First chance: pass this on to the system
            Last chance: display an appropriate error
        */
    } break;

    default: {
        /* Otherwise */
    } break;

    }
}


/* Add proper handler */
static BOOL s_enum_src_files(PSOURCEFILE src_file, PVOID ctx) {
    printf("Got source file: %s\n", src_file->FileName);
    return 1;
}


void lgdb_handle_create_process_debug_event(struct lgdb_process_ctx *ctx) {
    WIN32_CALL(SymInitialize, ctx->proc_info.hProcess, NULL, FALSE);

    ctx->process_pdb_base = SymLoadModule64(
        ctx->proc_info.hProcess,
        ctx->current_event.u.CreateProcessInfo.hFile,
        ctx->exe_path,
        0,
        (DWORD64)ctx->current_event.u.CreateProcessInfo.lpBaseOfImage,
        0);

    /* Determine if the symbols are available */
    ctx->module_info.SizeOfStruct = sizeof(ctx->module_info);
    BOOL success = WIN32_CALL(SymGetModuleInfo64,
        ctx->proc_info.hProcess,
        ctx->process_pdb_base,
        &ctx->module_info);

    if (success && ctx->module_info.SymType == SymPdb) {
        printf("Loaded \'%s\', symbols WERE loaded\n", ctx->exe_path);
    }
    else {
        printf("Loaded \'%s\', symbols WERE NOT loaded\n", ctx->exe_path);
    }

    // IMAGEHLP_SYMBOL64 *symbol = lgdb_make_symbol_info(ctx, "foo");
    // printf("foo is at %p\n", symbol->Address);
}


void lgdb_handle_exit_process_debug_event(struct lgdb_process_ctx *ctx) {
    /* TODO */
    ctx->is_running = 0;
}


void lgdb_handle_create_thread_debug_event(struct lgdb_process_ctx *ctx) {
    /* TODO */
}


void lgdb_handle_exit_thread_debug_event(struct lgdb_process_ctx *ctx) {
    /* TODO */
}


void lgdb_handle_load_dll_debug_event(struct lgdb_process_ctx *ctx) {
    const char *dll_name = lgdb_get_file_name_from_handle(ctx->current_event.u.LoadDll.hFile);

    DWORD64 base = SymLoadModule64(
        ctx->proc_info.hProcess,
        ctx->current_event.u.LoadDll.hFile,
        dll_name,
        0,
        (DWORD64)ctx->current_event.u.LoadDll.lpBaseOfDll,
        0);

    if (base) {
        printf("Loaded \'%s\', ", dll_name);
    }
    else {
        printf("Failed to load \'%s\'\n", dll_name);
    }

    // Code continues from above
    IMAGEHLP_MODULE64 module;
    module.SizeOfStruct = sizeof(module);
    BOOL success = WIN32_CALL(
        SymGetModuleInfo64,
        ctx->proc_info.hProcess,
        base,
        &module);

    lgdb_user_event_load_dll_t *data =
        LGDB_LNMALLOC(&ctx->events, lgdb_user_event_load_dll_t, 1);
    data->path = dll_name;
    data->symbols = success && (module.SymType == SymPdb);

    lgdb_trigger_user_event(ctx, LUET_LOAD_DLL, data, 0);

    // Check and notify
#if 0
    if (success && module.SymType == SymPdb) {
        printf("symbols WERE loaded\n");
    }
    else {
        printf("symbols WERE NOT loaded\n");
    }
#endif
}


void lgdb_handle_unload_dll_debug_event(struct lgdb_process_ctx *ctx) {
    /* TODO */
}


void lgdb_handle_output_debug_string_event(struct lgdb_process_ctx *ctx) {
    ctx->current_event.u.DebugString.lpDebugStringData;
    ctx->current_event.u.DebugString.nDebugStringLength;

    void *dst_ptr = malloc(ctx->current_event.u.DebugString.nDebugStringLength * sizeof(char));
    size_t bytes_read = 0;

    BOOL success = WIN32_CALL(ReadProcessMemory,
        ctx->proc_info.hProcess,
        ctx->current_event.u.DebugString.lpDebugStringData,
        dst_ptr,
        ctx->current_event.u.DebugString.nDebugStringLength,
        &bytes_read);

    printf("OutputDebugString: %s", (char *)dst_ptr);

    free(dst_ptr);
}


void lgdb_trigger_user_event(struct lgdb_process_ctx *ctx, uint32_t ev_type, void *ev_data, bool32_t require_input) {
    lgdb_user_event_t *ev = (lgdb_user_event_t *)lgdb_lnmalloc(&ctx->events, sizeof(lgdb_user_event_t));
    ev->ev_type = ev_type;
    ev->ev_data = ev_data;
    ev->next = NULL;

    /* No events have been pushed yet */
    if (!ctx->event_tail) {
        ctx->event_head = ctx->event_tail = ev;
    }
    else {
        ctx->event_head->next = ev;
    }

    ctx->triggered_user_event = 1;
    ctx->require_input |= require_input;
}
