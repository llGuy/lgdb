#include <stdio.h>
#include <assert.h>

#include "lgdb_table.h"
#include "lgdb_utility.h"
#include "lgdb_context.h"
#include "lgdb_breakpoint.h"


bool32_t lgdb_put_breakpoint_in_bin(
    struct lgdb_process_ctx *ctx,
    void *addr64,
    uint8_t *original) {
    uint8_t op_byte;
    size_t bytes_read;

    WIN32_CALL(
        ReadProcessMemory,
        ctx->proc_info.hProcess,
        addr64,
        &op_byte,
        1,
        &bytes_read);

    *original = op_byte;

    uint8_t int3 = LGDB_OP_INT3;
    size_t bytes_written;

    BOOL success = WIN32_CALL(
        WriteProcessMemory,
        ctx->proc_info.hProcess,
        addr64,
        &int3,
        1,
        &bytes_written);

    success &= WIN32_CALL(
        FlushInstructionCache,
        ctx->proc_info.hProcess,
        addr64,
        1);

    return (bool32_t)success;
}


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
        /* Actually set the op-code byte and push it to the ud_breakpoints data structure */
        lgdb_handle_t breakpoint_hdl = ctx->breakpoints.ud_breakpoint_count++;
        lgdb_breakpoint_t *breakpoint = &ctx->breakpoints.ud_breakpoints[breakpoint_hdl];
        lgdb_insert_in_tablep(&ctx->breakpoints.addr64_to_ud_idx, (void *)symbol->Address, breakpoint_hdl);

        breakpoint->addr = symbol->Address;
        breakpoint->file_name = NULL;

        uint8_t op_byte;
        success = (BOOL)lgdb_put_breakpoint_in_bin(
            ctx,
            symbol->Address,
            &op_byte);

        breakpoint->original_asm_op = op_byte;

        if (success) {
            printf(
                "Setting breakpoint at %s (%p), replaced 0x%.2X with 0x%.2X\n",
                symbol->Name,
                (void *)symbol->Address,
                (uint32_t)op_byte,
                (uint32_t)0xCC);
        }
    }
}


void lgdb_set_breakpointfl(
    struct lgdb_process_ctx *ctx,
    const char *file_name,
    uint32_t line_number) {
    /* SymGetLineFromName */
}


void lgdb_revert_to_original_byte(struct lgdb_process_ctx *ctx, lgdb_handle_t breakpoint_hdl) {
    assert(breakpoint_hdl != LGDB_INVALID_HANDLE);

    lgdb_breakpoint_t *breakpoint = &ctx->breakpoints.ud_breakpoints[breakpoint_hdl];

    uint8_t original_op_byte = breakpoint->original_asm_op;
    size_t bytes_written;

    BOOL success = WIN32_CALL(
        WriteProcessMemory,
        ctx->proc_info.hProcess,
        (void *)breakpoint->addr,
        &original_op_byte,
        1,
        &bytes_written);

    WIN32_CALL(
        FlushInstructionCache,
        ctx->proc_info.hProcess,
        (void *)breakpoint->addr,
        1);
}


void lgdb_preserve_breakpoint(struct lgdb_process_ctx *ctx, lgdb_handle_t breakpoint_hdl) {
    ctx->breakpoints.preserve_breakpoint = 1;
    ctx->breakpoints.breakpoint_to_preserve = breakpoint_hdl;

    /* Flag 8 corresponds to the trap flag which will trigger EXCEPTION_SINGLE_STEP */
    ctx->thread_ctx.EFlags |= (1 << 8);
}
