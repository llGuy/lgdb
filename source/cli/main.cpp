#include <stdio.h>
#include <chrono>

#include <Windows.h>
#include <DbgHelp.h>


extern "C" {

#include <lgdb_table.h>
#include <lgdb_step.h>
#include <lgdb_context.h>
#include <lgdb_symbol.h>

}


/* VERY basic input parsing for the moment */
static bool s_parse_input(lgdb_process_ctx_t *ctx);


using time_stamp_t = std::chrono::high_resolution_clock::time_point;
static time_stamp_t s_get_current_time();
static float s_get_time_difference(time_stamp_t end, time_stamp_t start);


int main() {
    /* Initialise context of the debugging process */
    lgdb_process_ctx_t *debug_ctx = lgdb_create_context(
        "C:\\Users\\lucro\\Development\\lgdb\\build\\Debug\\",
        "lgdbtest.exe");

    /* Example of setting breakpoints (before the process began) */
    lgdb_add_breakpointp(debug_ctx, "main");
    // lgdb_add_breakpointp(debug_ctx, "foo");
    lgdb_add_breakpointfl(debug_ctx, "main.cpp", 25);

    /* Start the process that is going to be debugged */
    lgdb_begin_process(debug_ctx);

    time_stamp_t now = s_get_current_time();

    /* Debugger loop */
    while (debug_ctx->is_running) {
        /* Poll all debug events and handle accordingly with callbacks */
        lgdb_user_event_t ev = {};
        bool got_event = lgdb_get_debug_event(debug_ctx, &ev);

        if (got_event) {
            /* In cases where execution has been suspended, and input is needed */
            if (debug_ctx->require_input) {
                /* should_continue will be true if we need to  */
                bool should_continue = false;
                while (!should_continue) {
                    float dt = s_get_time_difference(s_get_current_time(), now);
                    printf("%f ms elapsed\n", (dt * 1000.0f));

                    should_continue = s_parse_input(debug_ctx);

                    now = s_get_current_time();
                }
            }
            else {
                /* Make the process continue */
                lgdb_continue_process(debug_ctx);
            }
        }
    }

    /* Close all process handles */
    lgdb_close_process(debug_ctx);

    /* Deinitialise the debug ctx */
    lgdb_free_context(debug_ctx);

    return 0;
}


static bool s_parse_input(lgdb_process_ctx_t *ctx) {
    static char buffer[50] = { 0 };
    printf("(lgdb) ");
    int32_t ret = scanf("%s", buffer);

    switch (buffer[0]) {
    case 'n': {
        lgdb_single_source_step(ctx);
        lgdb_continue_process(ctx);
        return 1;
    } break;

    case 's': {
        lgdb_step_into(ctx);
        lgdb_continue_process(ctx);
        return 1;
    } break;

    case 'f': {
        lgdb_step_out(ctx);
        lgdb_continue_process(ctx);
        return 1;
    } break;

    case 't': {
        lgdb_update_call_stack(ctx);
    } break;

    case 'c': {
        lgdb_continue_process(ctx);
        return 1;
    } break;

    case 'q': {
        lgdb_terminate_process(ctx);
        return 0;
    } break;

    }

    return 0;
}


static time_stamp_t s_get_current_time() {
    return std::chrono::high_resolution_clock::now();
}


static float s_get_time_difference(time_stamp_t end, time_stamp_t start) {
    std::chrono::duration<float> seconds = end - start;
    float delta = seconds.count();
    return (float)delta;
}
