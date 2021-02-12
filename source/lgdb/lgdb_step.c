#include <stdio.h>
#include "lgdb_step.h"
#include "lgdb_symbol.h"
#include "lgdb_context.h"


void lgdb_single_asm_step(struct lgdb_process_ctx *ctx) {
    /* Trivial: set trap flag */
}


/* If check_for_call is set to true, then treat the call instruction just like a jump */
static s_prepare_step(struct lgdb_process_ctx *ctx, bool32_t check_for_call) {
    IMAGEHLP_LINE64 current_line = lgdb_make_line_info_from_addr(ctx, (void *)ctx->thread_ctx.Rip);
    IMAGEHLP_LINE64 next_line = lgdb_get_next_line_info(ctx, current_line);

    if (current_line.SizeOfStruct && next_line.SizeOfStruct) {
        ctx->breakpoints.previous_line = current_line.LineNumber;

        size_t bytes_read;

        uint64_t line_size = next_line.Address - ctx->thread_ctx.Rip;

        uint8_t *instr_buf = LGDB_LNMALLOC(&ctx->lnmem, uint8_t, line_size);
        WIN32_CALL(ReadProcessMemory, ctx->proc_info.hProcess, (void *)ctx->thread_ctx.Rip, instr_buf, line_size, &bytes_read);

        if (line_size == 1 && instr_buf[0] == 0xCC) {
            /* Are currently at a line with breakpoint */
            lgdb_entry_value_t *breakpoint_hdl = lgdb_get_from_tablep(
                &ctx->breakpoints.addr64_to_ud_idx,
                (void *)(ctx->thread_ctx.Rip));

            if (!breakpoint_hdl) {
                ++ctx->thread_ctx.Rip;
                lgdb_sync_process_thread_context(ctx);

                ++instr_buf;
                --line_size;
            }
        }

        /* Check for jump / ret instruction */
        lgdb_machine_instruction_t instr;
        uint64_t offset = 0;

        struct {
            uint8_t stop : 1;
            uint8_t next_line_is_break : 1;
        } flags;

        flags.stop = 0;
        flags.next_line_is_break = 0;
        ctx->breakpoints.is_checking_for_jump = 0;
        ctx->breakpoints.is_checking_for_call = 0;
        ctx->breakpoints.going_through_reloc = 0;
        ctx->breakpoints.set_jump_eflags = 0;

        while (!flags.stop && lgdb_decode_instruction_at(
            &ctx->dissasm,
            instr_buf + (uint32_t)offset,
            line_size - offset,
            &instr)) {
            switch (instr.mnemonic) {
            case ZYDIS_MNEMONIC_JB:    case ZYDIS_MNEMONIC_JBE:   case ZYDIS_MNEMONIC_JCXZ:
            case ZYDIS_MNEMONIC_JECXZ: case ZYDIS_MNEMONIC_JKNZD: case ZYDIS_MNEMONIC_JKZD:
            case ZYDIS_MNEMONIC_JL:    case ZYDIS_MNEMONIC_JLE:   case ZYDIS_MNEMONIC_JMP:
            case ZYDIS_MNEMONIC_JNB:   case ZYDIS_MNEMONIC_JNBE:  case ZYDIS_MNEMONIC_JNL:
            case ZYDIS_MNEMONIC_JNLE:  case ZYDIS_MNEMONIC_JNO:   case ZYDIS_MNEMONIC_JNP:
            case ZYDIS_MNEMONIC_JNS:   case ZYDIS_MNEMONIC_JNZ:   case ZYDIS_MNEMONIC_JO:
            case ZYDIS_MNEMONIC_JP:    case ZYDIS_MNEMONIC_JRCXZ: case ZYDIS_MNEMONIC_JS:
            case ZYDIS_MNEMONIC_JZ:    case ZYDIS_MNEMONIC_RET: {
                /* Put a breakpoint at this instruction */
                ctx->breakpoints.is_checking_for_jump = 1;

                uint64_t jump_addr = ctx->thread_ctx.Rip + offset;
                uint8_t original_op;
                lgdb_put_breakpoint_in_bin(ctx, (void *)(jump_addr), &original_op);
                ctx->breakpoints.single_step_breakpoint.addr = jump_addr;
                ctx->breakpoints.single_step_breakpoint.original_asm_op = original_op;
                ctx->breakpoints.jump_instr_len = instr.length;

                flags.stop = 1;
            } break;

            case ZYDIS_MNEMONIC_INT3: {
                lgdb_entry_value_t *breakpoint_hdl = lgdb_get_from_tablep(
                    &ctx->breakpoints.addr64_to_ud_idx,
                    (void *)(ctx->thread_ctx.Rip + offset));

                if (!breakpoint_hdl) {
                    assert(0);
                    flags.next_line_is_break = 1;
                    flags.stop = 1;
                }
            } break;

            case ZYDIS_MNEMONIC_CALL: {
                if (check_for_call) {
                    /* Put a breakpoint at this instruction */
                    ctx->breakpoints.is_checking_for_jump = 1;
                    ctx->breakpoints.is_checking_for_call = 1;

                    uint64_t jump_addr = ctx->thread_ctx.Rip + offset;
                    uint8_t original_op;
                    lgdb_put_breakpoint_in_bin(ctx, (void *)(jump_addr), &original_op);
                    ctx->breakpoints.single_step_breakpoint.addr = jump_addr;
                    ctx->breakpoints.single_step_breakpoint.original_asm_op = original_op;
                    ctx->breakpoints.jump_instr_len = instr.length;

                    ctx->breakpoints.call_end_address = jump_addr + instr.length;

                    flags.stop = 1;
                }
            } break;

            default: {

            } break;
            }

            offset += instr.length;
        }

        if (flags.next_line_is_break) {
            lgdb_clear_step_info(ctx);
        }
        else if (!ctx->breakpoints.is_checking_for_jump) {
            uint8_t next_line_first_instr;
            WIN32_CALL(ReadProcessMemory, ctx->proc_info.hProcess, (void *)next_line.Address, &next_line_first_instr, 1, &bytes_read);

            if (next_line_first_instr != 0xCC) {
                /* Put breakpoint at next instruction */
                uint8_t original_op;
                lgdb_put_breakpoint_in_bin(ctx, (void *)next_line.Address, &original_op);
                ctx->breakpoints.single_step_breakpoint.addr = next_line.Address;
                ctx->breakpoints.single_step_breakpoint.original_asm_op = original_op;
            }
        }
    }
    else {
        printf("Missing line information!\n");
    }
}


/* TODO: Optimise and clean */
void lgdb_single_source_step(struct lgdb_process_ctx *ctx) {
    s_prepare_step(ctx, 0);
}


void lgdb_step_into(struct lgdb_process_ctx *ctx) {
    s_prepare_step(ctx, 1);
}


void lgdb_clear_step_info(struct lgdb_process_ctx *ctx) {
    if (ctx->breakpoints.single_step_breakpoint.addr != ctx->thread_ctx.Rip &&
        ctx->breakpoints.single_step_breakpoint.addr) {
        lgdb_revert_to_original_byte(ctx, &ctx->breakpoints.single_step_breakpoint);
    }

    ctx->breakpoints.is_checking_for_jump = 0;
    ctx->breakpoints.is_checking_for_call = 0;
    ctx->breakpoints.going_through_reloc = 0;
    ctx->breakpoints.single_step_breakpoint.addr = 0;
    ctx->breakpoints.set_jump_eflags = 0;
}


void lgdb_step_out(struct lgdb_process_ctx *ctx) {
    uint64_t addr = lgdb_get_return_address(ctx);

    uint8_t ret_line_first_instr;
    size_t bytes_read;
    WIN32_CALL(ReadProcessMemory, ctx->proc_info.hProcess, (void *)addr, &ret_line_first_instr, 1, &bytes_read);

    if (ret_line_first_instr != 0xCC) {
        /* Put breakpoint at next instruction */
        uint8_t original_op;
        lgdb_put_breakpoint_in_bin(ctx, (void *)addr, &original_op);
        ctx->breakpoints.single_step_breakpoint.addr = addr;
        ctx->breakpoints.single_step_breakpoint.original_asm_op = original_op;
    }
}
