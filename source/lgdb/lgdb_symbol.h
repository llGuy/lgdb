#ifndef LGDB_SYMBOL_H
#define LGDB_SYMBOL_H

#include <stdint.h>

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

#endif