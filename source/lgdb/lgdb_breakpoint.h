#ifndef LGDB_BREAKPOINT_H
#define LGDB_BREAKPOINT_H

#include <stdint.h>
#include <lgdb_table.h>

#define LGDB_MAX_BREAKPOINTS 100
#define LGDB_OP_INT3 0xCC

typedef struct lgdb_breakpoint {
    uint64_t addr;
    const char *file_name;
    /* Haven't *yet* seen a source file with more than 16777215 lines of code... */
    uint32_t line_number : 24;
    uint32_t original_asm_op : 8;
} lgdb_breakpoint_t;

/* Breakpoints are accessed through the memory address of int3 (0xCC) */
typedef struct lgdb_breakpoints {
    /* User-defined breakpoints */
    uint32_t ud_breakpoint_count;
    lgdb_breakpoint_t ud_breakpoints[LGDB_MAX_BREAKPOINTS];

    /* Table which maps an address to a breakpoint */
    lgdb_table_t addr64_to_ud_idx;

    /* Invisible breakpoint (used for single stepping C/C++ code) */
    lgdb_breakpoint_t iv_breakpoint_single_step;
} lgdb_breakpoints_t;

/* Set breakpoint with function name */
void lgdb_set_breakpointp(struct lgdb_process_ctx *ctx, const char *function_name);
/* Set breakpoint with file name and line number */
void lgdb_set_breakpointfl(struct lgdb_process_ctx *ctx, const char *file_name, uint32_t line_number);
void lgdb_revert_to_original_byte(struct lgdb_process_ctx *ctx, lgdb_handle_t breakpoint_hdl);

#endif
