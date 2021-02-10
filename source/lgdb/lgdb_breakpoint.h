#ifndef LGDB_BREAKPOINT_H
#define LGDB_BREAKPOINT_H

#include <stdint.h>
#include <lgdb_table.h>

#define LGDB_MAX_BREAKPOINTS 100
#define LGDB_OP_INT3 0xCC

typedef struct lgdb_source_location {
    const char *file_name;
    uint32_t line_number;
    uint64_t addr;
} lgdb_source_location_t;

typedef struct lgdb_breakpoint {
    uint64_t addr;
    const char *file_name;
    /* Haven't *yet* seen a source file with more than 16777215 lines of code... */
    uint32_t line_number : 24;
    uint32_t original_asm_op : 8;
} lgdb_breakpoint_t;

enum lgdb_pending_breakpoint_type_t {
    /* With procedure name */
    LPBT_ADD_BREAKPOINTP,
    /* With file name and line number */
    LPBT_ADD_BREAKPOINTFL,
    LPBT_INVALID
};

typedef struct lgdb_pending_breakpoint {
    /* Depending on which lgdb_add_breakpoint* function was called */
    union {
        /* lgdb_add_breakpointp */
        struct {
            const char *function_name;
        } p;

        struct {
            const char *file_name;
            uint32_t line_number;
        } fl;
    };

    uint16_t called_function;
    uint16_t hdl;
} lgdb_pending_breakpoint_t;

/* Breakpoints are accessed through the memory address of int3 (0xCC) */
typedef struct lgdb_breakpoints {
    /* User-defined breakpoints */
    uint32_t ud_breakpoint_count;
    lgdb_breakpoint_t ud_breakpoints[LGDB_MAX_BREAKPOINTS];

    /* Breakpoints that were added but need to actually be put in the binary */
    uint16_t pending_breakpoint_count;
    lgdb_pending_breakpoint_t pending_breakpoints[LGDB_MAX_BREAKPOINTS / 2];

    /* Table which maps an address to a breakpoint */
    lgdb_table_t addr64_to_ud_idx;

    /* Invisible breakpoint (used for single stepping C/C++ code) */
    lgdb_breakpoint_t single_step_breakpoint;
    uint32_t previous_line; // From the single step instruction
    uint64_t call_end_address;

    lgdb_handle_t breakpoint_to_preserve;

    /* If it's 1, then the EXCEPTION_SINGLE_STEP will occur so that we can put the breakpoint back */
    uint32_t preserve_breakpoint : 1;
    /* Will be set to one if single-step detects a jump instruction */
    uint32_t is_checking_for_jump : 1;
    uint32_t is_checking_for_call : 1;
    uint32_t going_through_reloc : 1;
    uint32_t jump_instr_len : 5;
    /* Will use later */
    uint32_t other_flags : 24;
} lgdb_breakpoints_t;

/* 
    Pushes them to the pending list and will actually be added to the binary at lgdb_continue_process 
    Use this if you wish to add breakpoints before the process begins!
*/
void lgdb_add_breakpointp(struct lgdb_process_ctx *ctx, const char *function_name);
void lgdb_add_breakpointfl(struct lgdb_process_ctx *ctx, const char *file_name, uint32_t line_number);

/* Puts all the pending breakpoints in the binary */
void lgdb_flush_pending_breakpoints(struct lgdb_process_ctx *ctx);
/* 
    Actually adds the 0xCC in the binary (addr64 is in the address space of the debugged process) 
    without adding any thing the address to breakpoint table.
*/
bool32_t lgdb_put_breakpoint_in_bin(struct lgdb_process_ctx *ctx, void *addr64, uint8_t *original);

/* Set breakpoint with function name (the process needs to be running to use the lgdb_set_breakpoint* functions!) */
void lgdb_set_breakpointp(struct lgdb_process_ctx *ctx, const char *function_name);
/* Set breakpoint with file name and line number */
void lgdb_set_breakpointfl(struct lgdb_process_ctx *ctx, const char *file_name, uint32_t line_number);

void lgdb_revert_to_original_byte(struct lgdb_process_ctx *ctx, lgdb_breakpoint_t *breakpoint);
/* Sets preserve_breakpoint to 1, and sets the trap flag to 1 */
void lgdb_preserve_breakpoint(struct lgdb_process_ctx *ctx, lgdb_handle_t breakpoint_hdl);

void lgdb_handle_user_breakpoint(struct lgdb_process_ctx *ctx, lgdb_handle_t breakpoint_hdl);
/* __debugbreak() call */
void lgdb_handle_inline_breakpoint(struct lgdb_process_ctx *ctx);

#endif
