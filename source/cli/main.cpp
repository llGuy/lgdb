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

    /* Start the process that is going to be debugged */
    lgdb_begin_process(debug_ctx);

    /* Debugger loop */
    while (debug_ctx->is_running) {
        /* Poll all debug events and handle accordingly with callbacks */
        lgdb_poll_debug_events(debug_ctx);

        /* Make the process continue */
        lgdb_continue_process(debug_ctx);
    }

    /* Close all process handles */
    lgdb_close_process(debug_ctx);

    /* Deinitialise the debug ctx */
    lgdb_free_context(debug_ctx);

    return 0;
}