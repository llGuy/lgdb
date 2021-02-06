#ifndef LGDB_CONTEXT_H
#define LGDB_CONTEXT_H

#include <stdint.h>
#include <Windows.h>

#include "lgdb_symbol.h"
#include "lgdb_utility.h"
#include "lgdb_breakpoint.h"

void lgdb_test();

typedef struct lgdb_process_ctx {
    PROCESS_INFORMATION proc_info;
    DEBUG_EVENT current_event;

    /* TODO: Move these two to the modules part of the symbols struct */
    DWORD64 process_pdb_base;
    IMAGEHLP_MODULE64 module_info;
    
    lgdb_process_symbols_t symbols;
    lgdb_breakpoints_t breakpoints;

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
        };
    };
} lgdb_process_ctx_t;

lgdb_process_ctx_t *lgdb_create_context(const char *directory, const char *exe_name);
void lgdb_free_context(lgdb_process_ctx_t *ctx);
bool32_t lgdb_begin_process(lgdb_process_ctx_t *ctx);
void lgdb_close_process(lgdb_process_ctx_t *ctx);
void lgdb_poll_debug_events(lgdb_process_ctx_t *ctx);
void lgdb_continue_process(lgdb_process_ctx_t *ctx);

#endif