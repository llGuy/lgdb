#include "lgdb_step.h"
#include "lgdb_context.h"
#include "lgdb_breakpoint.h"
#include "lgdb_exception.h"


void lgdb_handle_access_violation_exception(struct lgdb_process_ctx *ctx) {

}


static void s_trigger_single_step_finished(lgdb_process_ctx_t *ctx, IMAGEHLP_LINE64 *line_info) {
    lgdb_user_event_source_code_step_finished_t *luescsf_data =
        LGDB_LNMALLOC(&ctx->events, lgdb_user_event_source_code_step_finished_t, 1);
    luescsf_data->file_name = line_info->FileName;
    luescsf_data->line_number = line_info->LineNumber;
    lgdb_trigger_user_event(ctx, LUET_SOURCE_CODE_STEP_FINISHED, luescsf_data, 1);
}


void lgdb_handle_breakpoint_exception(struct lgdb_process_ctx *ctx) {
    /* In all cases where a breakpoint was hit, decrement Rip */
    --ctx->thread_ctx.Rip;

    /* If breakpoint_hdl != NULL, this is a user-defined breakpoint */
    lgdb_entry_value_t *breakpoint_hdl = lgdb_get_from_tablep(
        &ctx->breakpoints.addr64_to_ud_idx,
        (void *)(ctx->thread_ctx.Rip));

    if (ctx->breakpoints.single_step_breakpoint.addr == ctx->thread_ctx.Rip) {
        if (ctx->breakpoints.is_checking_for_jump) {
            /* The single step breakpoint was set to check for a jump */
            lgdb_revert_to_original_byte(ctx, &ctx->breakpoints.single_step_breakpoint);

            ctx->thread_ctx.EFlags |= (1 << 8);
            ctx->breakpoints.set_jump_eflags = 1;
        }
        else {
            /* The single step breakpoint was just a regular single step breakpoint */
            lgdb_revert_to_original_byte(ctx, &ctx->breakpoints.single_step_breakpoint);

            lgdb_print_current_location(ctx);

            IMAGEHLP_LINE64 line_info = lgdb_make_line_info_from_addr(ctx, (void *)ctx->thread_ctx.Rip);
            if (line_info.SizeOfStruct)
                s_trigger_single_step_finished(ctx, &line_info);

            ctx->breakpoints.single_step_breakpoint.addr = 0;
        }
    }
    else if (breakpoint_hdl) {
        lgdb_handle_user_breakpoint(ctx, (lgdb_handle_t)*breakpoint_hdl);
    }
    else {
        lgdb_handle_inline_breakpoint(ctx);
    }

    lgdb_sync_process_thread_context(ctx);
}


/* This is for when we arrive to new line without going through a breakpoint */
static void s_handle_hitting_breakpoint(lgdb_process_ctx_t *ctx) {
    lgdb_entry_value_t *breakpoint_hdl = lgdb_get_from_tablep(
        &ctx->breakpoints.addr64_to_ud_idx,
        (void *)(ctx->thread_ctx.Rip));

    if (breakpoint_hdl) {
        lgdb_handle_user_breakpoint(ctx, (lgdb_handle_t)*breakpoint_hdl);
        lgdb_sync_process_thread_context(ctx);
    }
    else {
        lgdb_handle_inline_breakpoint(ctx);
    }
}


static void s_trigger_step_in_finished(lgdb_process_ctx_t *ctx, IMAGEHLP_LINE64 *line_info) {
    lgdb_user_event_step_in_finished_t *luesif_data =
        LGDB_LNMALLOC(&ctx->events, lgdb_user_event_step_in_finished_t, 1);
    luesif_data->file_name = line_info->FileName;
    luesif_data->line_number = line_info->LineNumber;
    lgdb_trigger_user_event(ctx, LUET_STEP_IN_FUNCTION_FINISHED, luesif_data, 1);
}


void lgdb_handle_single_step_exception(struct lgdb_process_ctx *ctx) {
    if (ctx->breakpoints.preserve_breakpoint) {
        /* Makes sure to put the breakpoint back in the binaries */
        lgdb_handle_t hdl = ctx->breakpoints.breakpoint_to_preserve;
        assert(hdl != LGDB_INVALID_HANDLE);

        lgdb_breakpoint_t *breakpoint = &ctx->breakpoints.ud_breakpoints[hdl];
        uint8_t op_byte;
        bool32_t success = lgdb_put_breakpoint_in_bin(ctx, (void *)breakpoint->addr, &op_byte);

        ctx->breakpoints.preserve_breakpoint = 0;
    }

    /* Only check for jumps if we have hit the jump breakpoint */
    if (ctx->breakpoints.set_jump_eflags) {
        if (ctx->thread_ctx.Rip - ctx->breakpoints.single_step_breakpoint.addr != ctx->breakpoints.jump_instr_len) {
            /* Sure to have made a jump */
            IMAGEHLP_LINE64 line_info = lgdb_make_line_info_from_addr(ctx, (void *)ctx->thread_ctx.Rip);
            if (line_info.SizeOfStruct) {
                ctx->breakpoints.single_step_breakpoint.addr = 0;
                ctx->breakpoints.is_checking_for_jump = 0;

                if (line_info.LineNumber == ctx->breakpoints.previous_line) {
                    /* Did a jump but wasn't taken to a different line (this is the case for for-loops for instance */
                    lgdb_single_source_step(ctx);
                }
                else {
                    /* Made a jump to a different line - check for breakpoints */
                    size_t bytes_read;
                    uint8_t op_byte = lgdb_read_current_op_byte(ctx, ctx->thread_ctx.Rip, &bytes_read);

                    if (op_byte == 0xCC) {
                        /* If breakpoint_hdl != NULL, this is a user-defined breakpoint */
                        s_handle_hitting_breakpoint(ctx);
                    }
                    else {
                        lgdb_print_current_location(ctx);

                        if (ctx->breakpoints.is_checking_for_call) {
                            s_trigger_step_in_finished(ctx, &line_info);
                            ctx->breakpoints.is_checking_for_call = 0;
                        }
                        else {
                            s_trigger_single_step_finished(ctx, &line_info);
                        }
                    }
                }
            }
            else if (ctx->breakpoints.is_checking_for_call) {
                if (ctx->breakpoints.going_through_reloc) {
                    ctx->breakpoints.going_through_reloc = 0;

                    /* Put breakpoint after the original call instruction if this is a function for which there are no debug info */
                    uint8_t original_op;
                    lgdb_put_breakpoint_in_bin(ctx, (void *)ctx->breakpoints.call_end_address, &original_op);
                    ctx->breakpoints.single_step_breakpoint.addr = ctx->breakpoints.call_end_address;
                    ctx->breakpoints.single_step_breakpoint.original_asm_op = original_op;
                }
                else {
                    ctx->breakpoints.going_through_reloc = 1;
                    ctx->thread_ctx.EFlags |= (1 << 8);
                    lgdb_sync_process_thread_context(ctx);
                }
            }
        }
        else {
            IMAGEHLP_LINE64 line_info = lgdb_make_line_info_from_addr(ctx, (void *)ctx->thread_ctx.Rip);

            /* Made a jump to a different line - check for breakpoints */
            size_t bytes_read;
            uint8_t op_byte = lgdb_read_current_op_byte(ctx, ctx->thread_ctx.Rip, &bytes_read);

            if (line_info.LineNumber == ctx->breakpoints.previous_line) {
                /* Step again it we haven't moved lines yet */
                lgdb_single_source_step(ctx);
            }
            else if (op_byte == 0xCC) {
                s_handle_hitting_breakpoint(ctx);
            }
            else {
                /* The step has finished */
                lgdb_print_current_location(ctx);

                s_trigger_single_step_finished(ctx, &line_info);
            }
        }
    }
}
