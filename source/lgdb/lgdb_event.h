#ifndef LGDB_EVENT_H
#define LGDB_EVENT_H

#include "lgdb_context.h"

void lgdb_handle_exception_debug_event(lgdb_process_ctx_t *ctx);
void lgdb_handle_create_process_debug_event(lgdb_process_ctx_t *ctx);
void lgdb_handle_exit_process_debug_event(lgdb_process_ctx_t *ctx);
void lgdb_handle_create_thread_debug_event(lgdb_process_ctx_t *ctx);
void lgdb_handle_exit_thread_debug_event(lgdb_process_ctx_t *ctx);
void lgdb_handle_load_dll_debug_event(lgdb_process_ctx_t *ctx);
void lgdb_handle_unload_dll_debug_event(lgdb_process_ctx_t *ctx);
void lgdb_handle_output_debug_string_event(lgdb_process_ctx_t *ctx);

/* Add option to add user-defined callbacks */

#endif
