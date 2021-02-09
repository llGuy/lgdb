#include <Windows.h>
#include <DbgHelp.h>
#include <stdio.h>
#include <assert.h>
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
        /* This breakpoint was set to check if we are going to jump */
        if (ctx->breakpoints.is_checking_for_jump) {
            lgdb_revert_to_original_byte(ctx, &ctx->breakpoints.jump_check_breakpoint);

            --ctx->thread_ctx.Rip;
            ctx->thread_ctx.EFlags |= (1 << 8);

            ctx->breakpoints.is_checking_for_jump = 0;
            ctx->breakpoints.expecting_single_step_for_jmp = 1;

            lgdb_sync_process_thread_context(ctx);
        }
        else {
            /* Check if the breakpoint was user-defined */
            lgdb_entry_value_t *breakpoint_hdl = lgdb_get_from_tablep(
                &ctx->breakpoints.addr64_to_ud_idx,
                (void *)(ctx->thread_ctx.Rip - 1));

            lgdb_update_call_stack(ctx);

            /* User defined breakpoint */
            if (breakpoint_hdl) {
                printf("BREAKPOINT at user-defined breakpoint\n");

                lgdb_breakpoint_t *breakpoint = &ctx->breakpoints.ud_breakpoints[*breakpoint_hdl];

                /* Revert int3 op byte to original byte */
                lgdb_revert_to_original_byte(ctx, breakpoint);

                /* Set trap flag so that we can put the breakpoint back! */
                lgdb_preserve_breakpoint(ctx, *breakpoint_hdl);

                --ctx->thread_ctx.Rip;

                lgdb_sync_process_thread_context(ctx);

                /* Trigger the breakpoint user event */
                IMAGEHLP_LINE64 line_info = lgdb_make_line_info_from_addr(ctx, (void *)ctx->thread_ctx.Rip);
                if (line_info.SizeOfStruct) {
                    lgdb_user_event_valid_breakpoint_hit_t *lvbh_data =
                        LGDB_LNMALLOC(&ctx->lnmem, lgdb_user_event_valid_breakpoint_hit_t, 1);
                    lvbh_data->file_name = line_info.FileName;
                    lvbh_data->line_number = line_info.LineNumber;
                    lgdb_trigger_user_event(ctx, LUET_VALID_BREAKPOINT_HIT, lvbh_data, 1);
                }

                ctx->breakpoints.is_single_src_code_step = 0;
            }
            /* TODO: If we hit breakpoint for single step */
            else if (ctx->breakpoints.is_single_src_code_step) {
                lgdb_revert_to_original_byte(ctx, &ctx->breakpoints.single_step_breakpoint);
                --ctx->thread_ctx.Rip;
                lgdb_sync_process_thread_context(ctx);

                IMAGEHLP_LINE64 line_info = lgdb_make_line_info_from_addr(ctx, (void *)ctx->thread_ctx.Rip);
                if (line_info.SizeOfStruct) {
                    lgdb_user_event_source_code_step_finished_t *luescsf_data =
                        LGDB_LNMALLOC(&ctx->lnmem, lgdb_user_event_source_code_step_finished_t, 1);
                    luescsf_data->file_name = line_info.FileName;
                    luescsf_data->line_number = line_info.LineNumber;
                    lgdb_trigger_user_event(ctx, LUET_SOURCE_CODE_STEP_FINISHED, luescsf_data, 1);
                }

                ctx->breakpoints.is_single_src_code_step = 0;
            }
            else {
                printf("BREAKPOINT at __debugbreak() call\n");

                IMAGEHLP_LINE64 line_info = lgdb_make_line_info_from_addr(ctx, (void *)ctx->thread_ctx.Rip);
                if (line_info.SizeOfStruct) {
                    lgdb_user_event_source_code_step_finished_t *luescsf_data =
                        LGDB_LNMALLOC(&ctx->lnmem, lgdb_user_event_source_code_step_finished_t, 1);
                    luescsf_data->file_name = line_info.FileName;
                    luescsf_data->line_number = line_info.LineNumber;
                    lgdb_trigger_user_event(ctx, LUET_SOURCE_CODE_STEP_FINISHED, luescsf_data, 1);
                }
            }
        }
    } break;

    case EXCEPTION_DATATYPE_MISALIGNMENT: {
        /*
            First chance: pass this on to the system
            Last chance: display an appropriate error
        */
    } break;

    case EXCEPTION_SINGLE_STEP: {
        if (ctx->breakpoints.preserve_breakpoint) {
            lgdb_handle_t hdl = ctx->breakpoints.breakpoint_to_preserve;
            assert(hdl != LGDB_INVALID_HANDLE);

            lgdb_breakpoint_t *breakpoint = &ctx->breakpoints.ud_breakpoints[hdl];
            uint8_t op_byte;
            bool32_t success = lgdb_put_breakpoint_in_bin(ctx, (void *)breakpoint->addr, &op_byte);

            ctx->breakpoints.preserve_breakpoint = 0;
        }
        else if (ctx->breakpoints.expecting_single_step_for_jmp) {
            if (ctx->thread_ctx.Rip < ctx->breakpoints.jump_check_breakpoint.addr ||
                ctx->thread_ctx.Rip - ctx->breakpoints.jump_check_breakpoint.addr != ctx->breakpoints.jump_instr_len) {
                printf("Made jump in single step instruction!\n");

                lgdb_update_call_stack(ctx);

                /* Sure to have made a jump */
                IMAGEHLP_LINE64 line_info = lgdb_make_line_info_from_addr(ctx, (void *)ctx->thread_ctx.Rip);
                if (line_info.SizeOfStruct) {
                    lgdb_user_event_source_code_step_finished_t *luescsf_data =
                        LGDB_LNMALLOC(&ctx->lnmem, lgdb_user_event_source_code_step_finished_t, 1);
                    luescsf_data->file_name = line_info.FileName;
                    luescsf_data->line_number = line_info.LineNumber;
                    lgdb_trigger_user_event(ctx, LUET_SOURCE_CODE_STEP_FINISHED, luescsf_data, 1);
                }

                ctx->breakpoints.is_single_src_code_step = 0;
                ctx->breakpoints.expecting_single_step_for_jmp = 0;

                /* Undo the invisible breakpoint that was set for next line instruction */
                if (ctx->breakpoints.single_step_breakpoint.addr)
                    lgdb_revert_to_original_byte(ctx, &ctx->breakpoints.single_step_breakpoint);
                lgdb_sync_process_thread_context(ctx);
            }
            else {
                /* Made no jump - can continue, will hit the breakpoint of the next source line */
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
