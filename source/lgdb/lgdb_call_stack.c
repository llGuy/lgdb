#include <stdio.h>
#include <Windows.h>
#include <DbgHelp.h>
#include "lgdb_context.h"
#include "lgdb_call_stack.h"


/* TODO: Actually update, don't just print */
void lgdb_update_call_stack(struct lgdb_process_ctx *ctx, void *obj, lgdb_update_call_stack_proc_t proc) {
    if (obj) {
        CONTEXT copy = ctx->thread_ctx;

        STACKFRAME64 stack = {
            .AddrPC.Offset = copy.Rip,
            .AddrPC.Mode = AddrModeFlat,
            .AddrFrame.Offset = copy.Rbp,
            .AddrFrame.Mode = AddrModeFlat,
            .AddrStack.Offset = copy.Rsp,
            .AddrStack.Mode = AddrModeFlat
        };

        do {
            BOOL success = WIN32_CALL(StackWalk64,
                IMAGE_FILE_MACHINE_AMD64,
                ctx->proc_info.hProcess,
                ctx->proc_info.hThread,
                &stack,
                &copy,
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
            IMAGEHLP_SYMBOL64 *symbol = (IMAGEHLP_SYMBOL64 *)lgdb_lnmalloc(&ctx->lnmem, sizeof(IMAGEHLP_SYMBOL64) + MAX_SYM_NAME);

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

            DWORD dw_displacement;

            success = WIN32_CALL(
                SymGetLineFromAddr64,
                ctx->proc_info.hProcess,
                stack.AddrPC.Offset,
                &dw_displacement,
                &line);

            lgdb_call_stack_frame_t frame = {
                .addr = stack.AddrPC.Offset,
                .file_name = line.FileName,
                .function_name = symbol->Name,
                .line_number = line.LineNumber,
                .module_name = module.ImageName
            };

            proc(ctx, obj, &frame);

            if (!ctx->load_call_stack_past_entry_point && ctx->entry_point.address == symbol->Address) {
                break;
            }

#if 0 // Old
            if (success) {
                printf("\t-> %s:%d (%p)\n", symbol->Name, line.LineNumber, (void *)copy.Rip);
            }
            else {
                printf("\t-> %s\n", symbol->Name);
            }
#endif

        } while (stack.AddrReturn.Offset != 0);
    }
}


void lgdb_print_current_location(struct lgdb_process_ctx *ctx) {
    IMAGEHLP_LINE64 line = {
        .SizeOfStruct = sizeof(line)
    };

    DWORD dw_displacement;

    BOOL success = WIN32_CALL(
        SymGetLineFromAddr64,
        ctx->proc_info.hProcess,
        ctx->thread_ctx.Rip,
        &dw_displacement,
        &line);

    printf("Stopped at: %s:%d\n", line.FileName, line.LineNumber);
}


/* If we know that we are stepping into a function, just push, instead of walking up the stack */
void lgdb_push_to_call_stack(struct lgdb_process_ctx *ctx) {
    /* TODO */
}


uint64_t lgdb_get_return_address(struct lgdb_process_ctx *ctx) {
    CONTEXT copy = ctx->thread_ctx;

    STACKFRAME64 stack = {
        .AddrPC.Offset = copy.Rip,
        .AddrPC.Mode = AddrModeFlat,
        .AddrFrame.Offset = copy.Rbp,
        .AddrFrame.Mode = AddrModeFlat,
        .AddrStack.Offset = copy.Rsp,
        .AddrStack.Mode = AddrModeFlat
    };

    BOOL success = WIN32_CALL(StackWalk64,
        IMAGE_FILE_MACHINE_AMD64,
        ctx->proc_info.hProcess,
        ctx->proc_info.hThread,
        &stack,
        &copy,
        lgdb_read_proc,
        SymFunctionTableAccess64,
        SymGetModuleBase64, 0);

    if (success) {
        return stack.AddrReturn.Offset;
    }
    else {
        return 0;
    }
}


void lgdb_get_stack_frame(struct lgdb_process_ctx *ctx, STACKFRAME64 *dst) {
    CONTEXT copy = ctx->thread_ctx;

    memset(dst, 0, sizeof(STACKFRAME64));
    dst->AddrPC.Offset = copy.Rip;
    dst->AddrPC.Mode = AddrModeFlat;
    dst->AddrFrame.Offset = copy.Rbp;
    dst->AddrFrame.Mode = AddrModeFlat;
    dst->AddrStack.Offset = copy.Rsp;
    dst->AddrStack.Mode = AddrModeFlat;

    BOOL success = WIN32_CALL(StackWalk64,
        IMAGE_FILE_MACHINE_AMD64,
        ctx->proc_info.hProcess,
        ctx->proc_info.hThread,
        dst,
        &copy,
        lgdb_read_proc,
        SymFunctionTableAccess64,
        SymGetModuleBase64, 0);
}
