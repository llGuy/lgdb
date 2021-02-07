#include <Windows.h>
#include <DbgHelp.h>
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
            First change: pass this on to the system
            Last chance: display an appropriate error
        */
    } break;

    case EXCEPTION_BREAKPOINT: {
        /*
            First change: display the current instruction
            and register values.
        */
        printf("BREAKPOINT HIT:\n");

        CONTEXT thread_ctx = {
            .ContextFlags = CONTEXT_FULL
        };

        BOOL success = WIN32_CALL(
            GetThreadContext,
            ctx->proc_info.hThread,
            &thread_ctx);

        STACKFRAME64 stack = {
            .AddrPC.Offset = thread_ctx.Rip,
            .AddrPC.Mode = AddrModeFlat,
            .AddrFrame.Offset = thread_ctx.Rbp,
            .AddrFrame.Mode = AddrModeFlat,
            .AddrStack.Offset = thread_ctx.Rsp,
            .AddrStack.Mode = AddrModeFlat
        };

        do {
            success = WIN32_CALL(StackWalk64,
                IMAGE_FILE_MACHINE_AMD64,
                ctx->proc_info.hProcess,
                ctx->proc_info.hThread,
                &stack,
                &thread_ctx,
                lgdb_read_proc,
                SymFunctionTableAccess64,
                SymGetModuleBase64, 0);

            IMAGEHLP_MODULE64 module = { 0 };
            module.SizeOfStruct = sizeof(module);
            WIN32_CALL(
                SymGetModuleInfo64,
                ctx->proc_info.hProcess,
                (DWORD64)stack.AddrPC.Offset,
                &module);

            // Read information in the stack structure and map to symbol file
            DWORD64 displacement;
            IMAGEHLP_SYMBOL64 *symbol = (IMAGEHLP_SYMBOL64 *)malloc(sizeof(IMAGEHLP_SYMBOL64) + MAX_SYM_NAME);

            memset(symbol, 0, sizeof(IMAGEHLP_SYMBOL64) + MAX_SYM_NAME);
            symbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
            symbol->MaxNameLength = MAX_SYM_NAME;

            WIN32_CALL(
                SymGetSymFromAddr64,
                ctx->proc_info.hProcess,
                stack.AddrPC.Offset,
                &displacement,
                symbol);

            IMAGEHLP_LINE64 line = {
                .SizeOfStruct = sizeof(line)
            };

            success = WIN32_CALL(
                SymGetLineFromAddr64,
                ctx->proc_info.hProcess,
                stack.AddrPC.Offset,
                &displacement,
                &line);

            if (success) {
                success = WIN32_CALL(SymGetLinePrev64, ctx->proc_info.hProcess, &line);

                printf("\t(%s:%p) %s (%s:%d)\n", module.ImageName, (void *)stack.AddrPC.Offset, symbol->Name, line.FileName, line.LineNumber);
            }
            else {
                printf("\t(%s:%p) %s\n", module.ImageName, (void *)stack.AddrPC.Offset, symbol->Name);
            }

        } while (stack.AddrReturn.Offset != 0);
    } break;

    case EXCEPTION_DATATYPE_MISALIGNMENT: {
        /*
            First change: pass this on to the system
            Last change: display an appropriate error
        */
    } break;

    case EXCEPTION_SINGLE_STEP: {
        /*
            First chance: pass this on to the system
            Last change: display an appropriate error
        */
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

    WIN32_CALL(
        SymEnumSourceFiles,
        ctx->proc_info.hProcess,
        ctx->process_pdb_base,
        "*",
        &s_enum_src_files,
        NULL);

    lgdb_set_breakpointp(ctx, "main");
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
