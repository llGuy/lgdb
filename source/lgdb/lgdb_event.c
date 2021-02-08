#include <Windows.h>
#include <DbgHelp.h>
#include <stdio.h>
#include <assert.h>
#include "lgdb_event.h"
#include "lgdb_utility.h"
#include "lgdb_context.h"


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
        lgdb_retrieve_thread_context(ctx);

        /* Check if the breakpoint was user-defined */
        lgdb_entry_value_t *breakpoint_hdl = lgdb_get_from_tablep(
            &ctx->breakpoints.addr64_to_ud_idx,
            (void *)(ctx->thread_ctx.Rip - 1));

        lgdb_update_call_stack(ctx);

        /* User defined breakpoint */
        if (breakpoint_hdl) {
            printf("BREAKPOINT at user-defined breakpoint\n");

            /* Revert int3 op byte to original byte */
            lgdb_revert_to_original_byte(ctx, *breakpoint_hdl);

            /* Set trap flag so that we can put the breakpoint back! */
            lgdb_preserve_breakpoint(ctx, *breakpoint_hdl);

            --ctx->thread_ctx.Rip;

            uint8_t instr_buf[15] = {0};
            size_t bytes_read;

            WIN32_CALL(
                ReadProcessMemory,
                ctx->proc_info.hProcess,
                (void *)ctx->thread_ctx.Rip,
                instr_buf,
                15,
                &bytes_read);

            lgdb_machine_instruction_t instr;
            lgdb_decode_instruction_at(&ctx->dissasm, instr_buf, 15, &instr);

            lgdb_sync_process_thread_context(ctx);
        }
        else {
            printf("BREAKPOINT at __debugbreak() call\n");
        }
    } break;

    case EXCEPTION_DATATYPE_MISALIGNMENT: {
        /*
            First chance: pass this on to the system
            Last chance: display an appropriate error
        */
    } break;

    case EXCEPTION_SINGLE_STEP: {
        lgdb_retrieve_thread_context(ctx);

        if (ctx->breakpoints.preserve_breakpoint) {
            lgdb_handle_t hdl = ctx->breakpoints.breakpoint_to_preserve;
            assert(hdl != LGDB_INVALID_HANDLE);

            lgdb_breakpoint_t *breakpoint = &ctx->breakpoints.ud_breakpoints[hdl];
            uint8_t op_byte;
            bool32_t success = lgdb_put_breakpoint_in_bin(ctx, (void *)breakpoint->addr, &op_byte);

            ctx->breakpoints.preserve_breakpoint = 0;
        }
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
        ctx->current_event.u.CreateProcessInfo.lpBaseOfImage,
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

    // Check and notify
    if (success && module.SymType == SymPdb) {
        printf("symbols WERE loaded\n");
    }
    else {
        printf("symbols WERE NOT loaded\n");
    }
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
