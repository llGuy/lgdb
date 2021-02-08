#ifndef LGDB_SYMBOL_H
#define LGDB_SYMBOL_H

#include <stdint.h>
#include <Windows.h>
#include <DbgHelp.h>

typedef struct lgdb_module_symbols {
    const char *path;
} lgdb_module_symbols_t;

typedef struct lgdb_process_symbols {
    /* 
        Contains more detailed information about the symbols of the modules 
        for instance some sort of line number information cache.
    */
    uint32_t module_count;
    lgdb_module_symbols_t *modules;
} lgdb_process_symbols_t;

IMAGEHLP_SYMBOL64 *lgdb_make_symbol_info(struct lgdb_process_ctx *ctx, const char *symbol_name);
IMAGEHLP_LINE64 lgdb_make_line_info(struct lgdb_process_ctx *ctx, const char *file_name, uint32_t line_number);
IMAGEHLP_LINE64 lgdb_make_line_info_from_addr(struct lgdb_process_ctx *ctx, void *addr);

#endif