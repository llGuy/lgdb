#ifndef LGDB_BREAKPOINT_H
#define LGDB_BREAKPOINT_H

#include <stdint.h>

typedef struct lgdb_breakpoints {
    /* User-defined breakpoints */
    uint32_t ud_breakpoint_count;

    /* Invisible breakpoints (used for single stepping C/C++ code) */
    uint32_t iv_breakpoint_count;
} lgdb_breakpoints_t;

#endif
