#ifndef LGDB_CALL_STACK_H
#define LGDB_CALL_STACK_H

#define LGDB_MAX_CALL_STACK_DEPTH 100

typedef struct lgdb_call_stack_frame {
    uint64_t addr;
} lgdb_call_stack_frame_t;

typedef struct lgdb_call_stack {
    uint32_t frame_count;
    lgdb_call_stack_frame_t frames[LGDB_MAX_CALL_STACK_DEPTH];
} lgdb_call_stack_t;

void lgdb_update_call_stack(struct lgdb_process_ctx *ctx);
void lgdb_print_current_location(struct lgdb_process_ctx *ctx);
/* If we know that we are stepping into a function, just push, instead of walking up the stack */
void lgdb_push_to_call_stack(struct lgdb_process_ctx *ctx);

#endif
