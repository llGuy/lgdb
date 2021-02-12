#define _NO_CVCONST_H

#include <stdio.h>
#include <Windows.h>
#include <DbgHelp.h>
#include "lgdb_symbol.h"
#include "lgdb_context.h"

#include <stdbool.h>
#include "TypeInfoStructs.h"

IMAGEHLP_SYMBOL64 *lgdb_make_symbol_info(
    struct lgdb_process_ctx *ctx,
    const char *symbol_name) {
    IMAGEHLP_SYMBOL64 *symbol = (IMAGEHLP_SYMBOL64 *)malloc(sizeof(IMAGEHLP_SYMBOL64) + MAX_SYM_NAME);

    memset(symbol, 0, sizeof(IMAGEHLP_SYMBOL64) + MAX_SYM_NAME);
    symbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
    symbol->MaxNameLength = MAX_SYM_NAME;

    BOOL success = WIN32_CALL(
        SymGetSymFromName64,
        ctx->proc_info.hProcess,
        symbol_name,
        symbol);

    if (!success) {
        free(symbol);
        symbol = NULL;
    }

    return symbol;
}


IMAGEHLP_LINE64 lgdb_make_line_info(
    struct lgdb_process_ctx *ctx,
    const char *file_name,
    uint32_t line_number) {
    IMAGEHLP_LINE64 line = {
        .SizeOfStruct = sizeof(line)
    };

    uint32_t displacement;
    BOOL success = WIN32_CALL(SymGetLineFromName64,
        ctx->proc_info.hProcess,
        ctx->module_info.ModuleName,
        file_name,
        line_number,
        &displacement,
        &line);

    if (!success) {
        memset(&line, 0, sizeof(line));
    }

    return line;
}


IMAGEHLP_LINE64 lgdb_make_line_info_from_addr(struct lgdb_process_ctx *ctx, void *addr) {
    IMAGEHLP_LINE64 line = {
        .SizeOfStruct = sizeof(line)
    };

    uint32_t displacement;
    BOOL success = WIN32_CALL(
        SymGetLineFromAddr64,
        ctx->proc_info.hProcess,
        (DWORD64)addr,
        &displacement,
        &line);

    if (!success) {
        memset(&line, 0, sizeof(line));
    }

    return line;
}


IMAGEHLP_LINE64 lgdb_get_next_line_info(struct lgdb_process_ctx *ctx, IMAGEHLP_LINE64 line) {
    WIN32_CALL(SymGetLineNext64, ctx->proc_info.hProcess, &line);
    return line;
}


static BOOL s_enum_symbols(
    PSYMBOL_INFO pSymInfo,
    ULONG SymbolSize,
    PVOID UserContext) {
    lgdb_process_ctx_t *ctx = (lgdb_process_ctx_t *)UserContext;

    printf("%s %d\n", pSymInfo->Name, pSymInfo->TypeIndex);

    DWORD tag;

    WIN32_CALL(SymGetTypeInfo, ctx->proc_info.hProcess, pSymInfo->ModBase, pSymInfo->TypeIndex, TI_GET_SYMTAG, &tag);

    if (tag == SymTagBaseType) {
        DWORD type;
        WIN32_CALL(SymGetTypeInfo, ctx->proc_info.hProcess, pSymInfo->ModBase, pSymInfo->TypeIndex, TI_GET_BASETYPE, &type);
        printf("%u\n", type);
    }

    // wchar_t *buffer = (wchar_t *)malloc(sizeof(wchar_t) * 100);
    // memset(buffer, 0, sizeof(wchar_t) * 100);

    // DWORD base_type;
    // WIN32_CALL(SymGetTypeInfo, ctx->proc_info.hProcess, pSymInfo->ModBase, type_id, TI_GET_BASETYPE, &base_type);

    SymTagBaseType;
    btInt;

    // char *b = (char *)buffer;

    printf("Got type\n");
}


BOOL s_enum_types(
    PSYMBOL_INFO pSymInfo,
    ULONG SymbolSize,
    PVOID UserContext) {
    printf("%s %d %d\n", pSymInfo->Name, pSymInfo->TypeIndex, pSymInfo->Index);
}



void lgdb_update_symbol_context(struct lgdb_process_ctx *ctx) {
    STACKFRAME64 frame;
    lgdb_get_stack_frame(ctx, &frame);

    IMAGEHLP_STACK_FRAME stack_frame = {
        .InstructionOffset = ctx->thread_ctx.Rip,
        .ReturnOffset = frame.AddrReturn.Offset,
        .FrameOffset = ctx->thread_ctx.Rbp,
        .StackOffset = ctx->thread_ctx.Rsp,
        .Virtual = FALSE
    };

    // WIN32_CALL(SymEnumTypes, ctx->proc_info.hProcess, ctx->process_pdb_base, s_enum_types, ctx);

    WIN32_CALL(SymSetContext, ctx->proc_info.hProcess, &stack_frame, NULL);

    WIN32_CALL(SymEnumSymbols, ctx->proc_info.hProcess, 0, 0, s_enum_symbols, ctx);

    stack_frame.InstructionOffset = ctx->process_pdb_base;
    
    // WIN32_CALL(SymSetContext, ctx->proc_info.hProcess, &stack_frame, NULL);

    ULONG type_index = 7;
    wchar_t *buffer = LGDB_LNMALLOC(&ctx->lnmem, wchar_t, 100);
    memset(buffer, 0, sizeof(wchar_t) * 100);
    DWORD type_id;
    WIN32_CALL(SymGetTypeInfo, ctx->proc_info.hProcess, ctx->process_pdb_base, type_index, TI_GET_SYMNAME, &buffer);

    type_index = 9;
    WIN32_CALL(SymGetTypeInfo, ctx->proc_info.hProcess, ctx->process_pdb_base, type_index, TI_GET_SYMNAME, &buffer);

    type_index = 11;
    WIN32_CALL(SymGetTypeInfo, ctx->proc_info.hProcess, ctx->process_pdb_base, type_index, TI_GET_SYMNAME, &buffer);

    printf("HI\n");
}
