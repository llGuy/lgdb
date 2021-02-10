#ifndef LGDB_STEP_H
#define LGDB_STEP_H

#include <stdint.h>

#if 0
typedef struct lgdb_stepping {
    uint32_t is_checking_jump : 1;
} lgdb_stepping_t;
#endif

/* Single assembly instruction step */
void lgdb_single_asm_step(struct lgdb_process_ctx *ctx);
/* Single source code instruction step */
void lgdb_single_source_step(struct lgdb_process_ctx *ctx);
void lgdb_step_into(struct lgdb_process_ctx *ctx);
void lgdb_step_out(struct lgdb_process_ctx *ctx);

void lgdb_clear_step_info(struct lgdb_process_ctx *ctx);

#endif
