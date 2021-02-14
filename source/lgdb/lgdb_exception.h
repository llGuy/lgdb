#ifndef LGDB_EXCEPTION_H
#define LGDB_EXCEPTION_H

void lgdb_handle_access_violation_exception(struct lgdb_process_ctx *ctx);
void lgdb_handle_breakpoint_exception(struct lgdb_process_ctx *ctx);
void lgdb_handle_single_step_exception(struct lgdb_process_ctx *ctx);

#endif