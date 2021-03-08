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

typedef struct lgdb_entry_point {
    const char *function_name;
    uint64_t address;
} lgdb_entry_point_t;

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
    lgdb_linear_allocator_t lnmem;
    lgdb_entry_point_t entry_point;

    lgdb_linear_allocator_t events;
    lgdb_user_event_t *event_head;
    lgdb_user_event_t *event_tail;
    bool32_t received_event;

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
            uint32_t block_on_wait : 1;
            uint32_t load_call_stack_past_entry_point : 1;
        };
    };
} lgdb_process_ctx_t;

lgdb_process_ctx_t *lgdb_create_context();
/* directory = path/to/folder/  exe = myprogram.exe */
/* (note, full path would be: path/to/folder/myprogram.exe */
void lgdb_open_process_context(lgdb_process_ctx_t *ctx, const char *directory, const char *exe);
/* Simply resets default values and gets ready to initialise the context again */
void lgdb_close_process_context(lgdb_process_ctx_t *ctx);
void lgdb_free_context(lgdb_process_ctx_t *ctx);
bool32_t lgdb_begin_process(lgdb_process_ctx_t *ctx);
bool32_t lgdb_terminate_process(lgdb_process_ctx_t *ctx);
void lgdb_close_process(lgdb_process_ctx_t *ctx);
/* Pass INFINITE for no timeout */
void lgdb_get_debug_event(lgdb_process_ctx_t *ctx, uint32_t timout);
bool32_t lgdb_translate_debug_event(lgdb_process_ctx_t *ctx);
void lgdb_clear_events(lgdb_process_ctx_t *ctx);
/* Also does things like flushing all pending breakpoints */
void lgdb_continue_process(lgdb_process_ctx_t *ctx);
BOOL lgdb_retrieve_thread_context(lgdb_process_ctx_t *ctx);
void lgdb_sync_process_thread_context(lgdb_process_ctx_t *ctx);
void lgdb_print_diagnostics(lgdb_process_ctx_t *ctx);
lgdb_entry_point_t lgdb_find_entry_point(lgdb_process_ctx_t *ctx);

#endif