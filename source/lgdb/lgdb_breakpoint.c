#include <stdio.h>

#include "lgdb_utility.h"
#include "lgdb_context.h"
#include "lgdb_breakpoint.h"


void lgdb_set_breakpointp(
    struct lgdb_process_ctx *ctx,
    const char *function_name) {
    IMAGEHLP_SYMBOL64 *symbol = (IMAGEHLP_SYMBOL64 *)malloc(sizeof(IMAGEHLP_SYMBOL64) + MAX_SYM_NAME);

    memset(symbol, 0, sizeof(IMAGEHLP_SYMBOL64) + MAX_SYM_NAME);
    symbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
    symbol->MaxNameLength = MAX_SYM_NAME;

    BOOL success = WIN32_CALL(
        SymGetSymFromName64,
        ctx->proc_info.hProcess,
        function_name,
        symbol);

    if (success) {
        printf("Setting breakpoint at %s (%p)\n", symbol->Name, (void *)symbol->Address);
        /* Actually set the op-code byte and push it to the ud_breakpoints data structure */
    }
}


void lgdb_set_breakpointfl(
    struct lgdb_process_ctx *ctx,
    const char *file_name,
    uint32_t line_number) {
    /* SymGetLineFromName */
}
