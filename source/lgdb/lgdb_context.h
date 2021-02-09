#ifndef LGDB_CONTEXT_H
#define LGDB_CONTEXT_H

#include <stdint.h>
#include <Windows.h>
#include <DbgHelp.h>

#include "lgdb_alloc.h"
#include "lgdb_event.h"
#include "lgdb_symbol.h"
#include "lgdb_dissasm.h"
#include "lgdb_utility.h"
#include "lgdb_breakpoint.h"
#include "lgdb_call_stack.h"

typedef struct lgdb_process_ctx {
    PROCESS_INFORMATION proc_info;
    DEBUG_EVENT current_event;
    /* Gets updated each time an event that needs it occurs */
    CONTEXT thread_ctx;

    /* TODO: Move these two to the modules part of the symbols struct */
    DWORD64 process_pdb_base;
    IMAGEHLP_MODULE64 module_info;
    
    lgdb_process_symbols_t symbols;
    lgdb_breakpoints_t breakpoints;
    lgdb_call_stack_t call_stack;
    lgdb_dissasm_t dissasm;
    lgdb_user_event_t current_user_event;
    lgdb_linear_allocator_t lnmem;

    union {
        const char *exe_path;
        struct {
            const char *directory;
            const char *exe_name;
        };
    };

    union {
        uint32_t flags;
        struct {
            uint32_t is_running : 1;
            uint32_t triggered_user_event : 1;
            uint32_t require_input : 1;
        };
    };
} lgdb_process_ctx_t;

lgdb_process_ctx_t *lgdb_create_context(const char *directory, const char *exe_name);
void lgdb_free_context(lgdb_process_ctx_t *ctx);
bool32_t lgdb_begin_process(lgdb_process_ctx_t *ctx);
void lgdb_close_process(lgdb_process_ctx_t *ctx);
bool32_t lgdb_get_debug_event(lgdb_process_ctx_t *ctx, lgdb_user_event_t *dst);
/* Also does things like flushing all pending breakpoints */
void lgdb_continue_process(lgdb_process_ctx_t *ctx);
BOOL lgdb_retrieve_thread_context(lgdb_process_ctx_t *ctx);
void lgdb_sync_process_thread_context(lgdb_process_ctx_t *ctx);

#endif