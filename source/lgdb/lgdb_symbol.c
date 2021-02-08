#include <Windows.h>
#include <DbgHelp.h>
#include "lgdb_symbol.h"
#include "lgdb_context.h"


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
