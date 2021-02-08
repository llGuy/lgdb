#include <stdio.h>

#include <Windows.h>
#include <DbgHelp.h>


extern "C" {

#include <lgdb_table.h>
#include <lgdb_context.h>

}


int main() {
    /* Initialise context of the debugging process */
    lgdb_process_ctx_t *debug_ctx = lgdb_create_context(
        "C:\\Users\\lucro\\Development\\lgdb\\build\\Debug\\",
        "lgdbtest.exe");

    /* Example of setting breakpoints (before the process began) */
    lgdb_add_breakpointp(debug_ctx, "main");
    lgdb_add_breakpointp(debug_ctx, "foo");
    lgdb_add_breakpointfl(debug_ctx, "main.cpp", 28);

    /* Start the process that is going to be debugged */
    lgdb_begin_process(debug_ctx);

    /* Debugger loop */
    while (debug_ctx->is_running) {
        /* Poll all debug events and handle accordingly with callbacks */
        lgdb_user_event_t ev = {};
        bool got_event = lgdb_get_debug_event(debug_ctx, &ev);

        if (got_event) {
            /* should_continue will be true if we need to  */
            bool should_continue = false;
            while (!should_continue) {

            }
        }

        /* Make the process continue */
        lgdb_continue_process(debug_ctx);
    }

    /* Close all process handles */
    lgdb_close_process(debug_ctx);

    /* Deinitialise the debug ctx */
    lgdb_free_context(debug_ctx);

    return 0;
}