#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <chrono>

#define _NO_CVCONST_H
#include <Windows.h>
#include <DbgHelp.h>

#include <stdbool.h>
#include <TypeInfoStructs.h>


#include <tuple>


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
    lgdb_process_ctx_t *debug_ctx = lgdb_create_context();

#if 1
    lgdb_open_process_context(
        debug_ctx,
        "C:\\Users\\lucro\\Development\\lgdb\\build\\Debug\\",
        "lgdbtest.exe");
#else
    lgdb_open_process_context(
        debug_ctx,
        "C:\\Users\\lucro\\Development\\vkPhysics\\build\\Debug\\",
        "vkPhysics_client.exe");
#endif

    /* Example of setting breakpoints (before the process began) */
    lgdb_add_breakpointp(debug_ctx, "main");
    // lgdb_add_breakpointfl(debug_ctx, "main.cpp", 28);

    /* Start the process that is going to be debugged */
    lgdb_begin_process(debug_ctx);

    time_stamp_t now = s_get_current_time();

    /* Debugger loop */
    while (debug_ctx->is_running) {
        lgdb_clear_events(debug_ctx);

        /* Poll all debug events and handle accordingly with callbacks */
        lgdb_get_debug_event(debug_ctx, INFINITE);
        bool got_event = lgdb_translate_debug_event(debug_ctx);

        if (got_event) {
            bool should_continue = 1;

            for (lgdb_user_event_t *ev = debug_ctx->event_tail; ev; ev = ev->next) {
                /* In cases where execution has been suspended, and input is needed */
                if (debug_ctx->require_input) {
                    lgdb_update_symbol_context(debug_ctx);
                    lgdb_update_local_symbols_depr(debug_ctx);

                    /* should_continue will be true if we need to  */
                    bool should_continue = false;
                    while (!should_continue) {
                        float dt = s_get_time_difference(s_get_current_time(), now);
                        printf("%f ms elapsed\n", (dt * 1000.0f));

                        should_continue = s_parse_input(debug_ctx);

                        now = s_get_current_time();
                    }

                    should_continue = 0;
                }
            }

            if (should_continue)
                lgdb_continue_process(debug_ctx);
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
    static char buffer1[50] = { 0 };
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
        lgdb_update_call_stack(ctx, NULL, NULL);
    } break;

    case 'c': {
        lgdb_continue_process(ctx);
        return 1;
    } break;

    case 'p': {
        int32_t ret = scanf("%s", buffer);
        lgdb_print_symbol_value(ctx, buffer);
        putchar('\n');

        return 0;
    } break;

    case 'q': {
        lgdb_terminate_process(ctx);
        return 0;
    } break;

        /* Diagnostics: */
    case 'd': {
        lgdb_print_diagnostics(ctx);
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
