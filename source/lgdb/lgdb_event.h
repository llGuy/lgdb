#ifndef LGDB_EVENT_H
#define LGDB_EVENT_H

void lgdb_handle_exception_debug_event(struct lgdb_process_ctx *ctx);
void lgdb_handle_create_process_debug_event(struct lgdb_process_ctx *ctx);
void lgdb_handle_exit_process_debug_event(struct lgdb_process_ctx *ctx);
void lgdb_handle_create_thread_debug_event(struct lgdb_process_ctx *ctx);
void lgdb_handle_exit_thread_debug_event(struct lgdb_process_ctx *ctx);
void lgdb_handle_load_dll_debug_event(struct lgdb_process_ctx *ctx);
void lgdb_handle_unload_dll_debug_event(struct lgdb_process_ctx *ctx);
void lgdb_handle_output_debug_string_event(struct lgdb_process_ctx *ctx);

/* Add option to add user-defined callbacks */

#endif
