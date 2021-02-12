#include <Windows.h>
#include <DbgHelp.h>
#include <stdio.h>
#include <assert.h>
#include "lgdb_step.h"
#include "lgdb_event.h"
#include "lgdb_symbol.h"
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
                if (line_info.SizeOfStruct) {
                    lgdb_user_event_source_code_step_finished_t *luescsf_data =
                        LGDB_LNMALLOC(&ctx->lnmem, lgdb_user_event_source_code_step_finished_t, 1);
                    luescsf_data->file_name = line_info.FileName;
                    luescsf_data->line_number = line_info.LineNumber;
                    lgdb_trigger_user_event(ctx, LUET_SOURCE_CODE_STEP_FINISHED, luescsf_data, 1);
                }

                ctx->breakpoints.single_step_breakpoint.addr = 0;
            }
        }
        else if (breakpoint_hdl) {
            lgdb_handle_user_breakpoint(ctx, *breakpoint_hdl);
        }
        else {
            lgdb_handle_inline_breakpoint(ctx);
        }

        lgdb_sync_process_thread_context(ctx);
    } break;

    case EXCEPTION_DATATYPE_MISALIGNMENT: {
        /*
            First chance: pass this on to the system
            Last chance: display an appropriate error
        */
    } break;

    case EXCEPTION_SINGLE_STEP: {
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
        if (ctx->breakpoints.is_checking_for_jump && ctx->breakpoints.set_jump_eflags) {
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
                        uint8_t op_byte;
                        size_t bytes_read;

                        WIN32_CALL(
                            ReadProcessMemory,
                            ctx->proc_info.hProcess,
                            (void *)ctx->thread_ctx.Rip,
                            &op_byte,
                            1,
                            &bytes_read);

                        if (op_byte == 0xCC) {
                            /* If breakpoint_hdl != NULL, this is a user-defined breakpoint */
                            lgdb_entry_value_t *breakpoint_hdl = lgdb_get_from_tablep(
                                &ctx->breakpoints.addr64_to_ud_idx,
                                (void *)(ctx->thread_ctx.Rip));

                            if (breakpoint_hdl) {
                                lgdb_handle_user_breakpoint(ctx, *breakpoint_hdl);
                                lgdb_sync_process_thread_context(ctx);
                            }
                            else {
                                lgdb_handle_inline_breakpoint(ctx);
                            }
                        }
                        else {
                            lgdb_print_current_location(ctx);

                            if (ctx->breakpoints.is_checking_for_call) {
                                lgdb_user_event_step_in_finished_t *luesif_data =
                                    LGDB_LNMALLOC(&ctx->lnmem, lgdb_user_event_step_in_finished_t, 1);
                                luesif_data->file_name = line_info.FileName;
                                luesif_data->line_number = line_info.LineNumber;
                                lgdb_trigger_user_event(ctx, LUET_STEP_IN_FUNCTION_FINISHED, luesif_data, 1);

                                ctx->breakpoints.is_checking_for_call = 0;
                            }
                            else {
                                lgdb_user_event_source_code_step_finished_t *luescsf_data =
                                    LGDB_LNMALLOC(&ctx->lnmem, lgdb_user_event_source_code_step_finished_t, 1);
                                luescsf_data->file_name = line_info.FileName;
                                luescsf_data->line_number = line_info.LineNumber;
                                lgdb_trigger_user_event(ctx, LUET_SOURCE_CODE_STEP_FINISHED, luescsf_data, 1);
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
                uint8_t op_byte;
                size_t bytes_read;

                WIN32_CALL(
                    ReadProcessMemory,
                    ctx->proc_info.hProcess,
                    (void *)ctx->thread_ctx.Rip,
                    &op_byte,
                    1,
                    &bytes_read);

                if (line_info.LineNumber == ctx->breakpoints.previous_line) {
                    /* Step again it we haven't moved lines yet */
                    lgdb_single_source_step(ctx);
                }
                else if (op_byte == 0xCC) {
                    /* If breakpoint_hdl != NULL, this is a user-defined breakpoint */
                    lgdb_entry_value_t *breakpoint_hdl = lgdb_get_from_tablep(
                        &ctx->breakpoints.addr64_to_ud_idx,
                        (void *)(ctx->thread_ctx.Rip));

                    if (breakpoint_hdl) {
                        lgdb_handle_user_breakpoint(ctx, *breakpoint_hdl);
                        lgdb_sync_process_thread_context(ctx);
                    }
                    else {
                        lgdb_handle_inline_breakpoint(ctx);
                    }
                }
                else {
                    /* The step has finished */
                    lgdb_print_current_location(ctx);

                    lgdb_user_event_source_code_step_finished_t *luescsf_data =
                        LGDB_LNMALLOC(&ctx->lnmem, lgdb_user_event_source_code_step_finished_t, 1);
                    luescsf_data->file_name = line_info.FileName;
                    luescsf_data->line_number = line_info.LineNumber;
                    lgdb_trigger_user_event(ctx, LUET_SOURCE_CODE_STEP_FINISHED, luescsf_data, 1);
                }
            }
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

    IMAGEHLP_SYMBOL64 *symbol = lgdb_make_symbol_info(ctx, "foo");
    printf("foo is at %p\n", symbol->Address);
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


void lgdb_trigger_user_event(struct lgdb_process_ctx *ctx, uint32_t ev_type, void *ev_data, bool32_t require_input) {
    ctx->triggered_user_event = 1;
    ctx->require_input = require_input;
    ctx->current_user_event.ev_type = ev_type;
    ctx->current_user_event.ev_data = ev_data;
}
