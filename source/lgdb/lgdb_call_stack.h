#ifndef LGDB_CALL_STACK_H
#define LGDB_CALL_STACK_H

#define LGDB_MAX_CALL_STACK_DEPTH 100

typedef struct lgdb_call_stack_frame {
    uint64_t addr;
    const char *function_name;
    const char *file_name;
    const char *module_name;
    int32_t line_number;
} lgdb_call_stack_frame_t;

typedef struct lgdb_call_stack {
    uint32_t frame_count;
    lgdb_call_stack_frame_t frames[LGDB_MAX_CALL_STACK_DEPTH];
} lgdb_call_stack_t;

typedef void (* lgdb_update_call_stack_proc_t)(
    struct lgdb_process_ctx *ctx,
    void *obj,
    lgdb_call_stack_frame_t *frame);

void lgdb_update_call_stack(struct lgdb_process_ctx *ctx, void *obj, lgdb_update_call_stack_proc_t proc);
void lgdb_print_current_location(struct lgdb_process_ctx *ctx);

/* If we know that we are stepping into a function, just push, instead of walking up the stack */
void lgdb_push_to_call_stack(struct lgdb_process_ctx *ctx);

uint64_t lgdb_get_return_address(struct lgdb_process_ctx *ctx);
void lgdb_get_stack_frame(struct lgdb_process_ctx *ctx, STACKFRAME64 *dst);

#endif
