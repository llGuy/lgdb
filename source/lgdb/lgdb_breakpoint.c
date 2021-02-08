#include <stdio.h>
#include <assert.h>

#include "lgdb_table.h"
#include "lgdb_utility.h"
#include "lgdb_context.h"
#include "lgdb_breakpoint.h"


static IMAGEHLP_SYMBOL64 *s_make_symbol_info(
    lgdb_process_ctx_t *ctx,
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


static void s_free_symbol_info(IMAGEHLP_SYMBOL64 *symbol) {
    free(symbol);
}


void lgdb_add_breakpointp(struct lgdb_process_ctx *ctx, const char *function_name) {
    lgdb_handle_t breakpoint_hdl = ctx->breakpoints.ud_breakpoint_count++;
    lgdb_breakpoint_t *breakpoint = &ctx->breakpoints.ud_breakpoints[breakpoint_hdl];

    breakpoint->addr = NULL;
    /* TODO: Add this information */
    breakpoint->file_name = NULL;
    breakpoint->line_number = 0;
    breakpoint->original_asm_op = 0;

    uint16_t pending = ctx->breakpoints.pending_breakpoint_count++;
    lgdb_pending_breakpoint_t *pend = &ctx->breakpoints.pending_breakpoints[pending];
    pend->hdl = (uint16_t)breakpoint_hdl;
    pend->called_function = LPBT_ADD_BREAKPOINTP;
    pend->p.function_name = function_name;
}


void lgdb_add_breakpointfl(struct lgdb_process_ctx *ctx, const char *file_name, uint32_t line_number) {
    /* TODO */
}


void lgdb_flush_pending_breakpoints(struct lgdb_process_ctx *ctx) {
    for (uint32_t i = 0; i < ctx->breakpoints.pending_breakpoint_count; ++i) {
        lgdb_pending_breakpoint_t *pend = &ctx->breakpoints.pending_breakpoints[i];
        lgdb_breakpoint_t *breakpoint = &ctx->breakpoints.ud_breakpoints[pend->hdl];

        /* Retrieve this addr64 */
        switch (pend->called_function) {

        case LPBT_ADD_BREAKPOINTP: {
            IMAGEHLP_SYMBOL64 *symbol = s_make_symbol_info(ctx, pend->p.function_name);
            breakpoint->addr = symbol->Address;

            /* TODO: Figure out what to do before freeing the symbol structure */
        } break;

        case LPBT_ADD_BREAKPOINTFL: {
            /* TODO */
        } break;

        default: {
            printf("Used unrecognised function to add breakpoint\n");
            assert(0);
        } break;

        }

        lgdb_insert_in_tablep(&ctx->breakpoints.addr64_to_ud_idx, (void *)breakpoint->addr, pend->hdl);

        uint8_t op_byte;
        BOOL success = (BOOL)lgdb_put_breakpoint_in_bin(
            ctx,
            breakpoint->addr,
            &op_byte);

        breakpoint->original_asm_op = op_byte;

        if (success) {
            printf(
                "Setting breakpoint at (%p), replaced 0x%.2X with 0x%.2X\n",
                (void *)breakpoint->addr,
                (uint32_t)op_byte,
                (uint32_t)0xCC);
        }
    }

    ctx->breakpoints.pending_breakpoint_count = 0;
}


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
    IMAGEHLP_SYMBOL64 *symbol = s_make_symbol_info(ctx, function_name);

    if (symbol) {
        /* Actually set the op-code byte and push it to the ud_breakpoints data structure */
        lgdb_handle_t breakpoint_hdl = ctx->breakpoints.ud_breakpoint_count++;
        lgdb_breakpoint_t *breakpoint = &ctx->breakpoints.ud_breakpoints[breakpoint_hdl];
        lgdb_insert_in_tablep(&ctx->breakpoints.addr64_to_ud_idx, (void *)symbol->Address, breakpoint_hdl);

        breakpoint->addr = symbol->Address;
        breakpoint->file_name = NULL;

        uint8_t op_byte;
        BOOL success = (BOOL)lgdb_put_breakpoint_in_bin(
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
