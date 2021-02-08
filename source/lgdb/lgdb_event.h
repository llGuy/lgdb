#ifndef LGDB_EVENT_H
#define LGDB_EVENT_H

#include <stdint.h>

/* Handle the system-level debug events (will most likely trigger user-level event) */
void lgdb_handle_exception_debug_event(struct lgdb_process_ctx *ctx);
void lgdb_handle_create_process_debug_event(struct lgdb_process_ctx *ctx);
void lgdb_handle_exit_process_debug_event(struct lgdb_process_ctx *ctx);
void lgdb_handle_create_thread_debug_event(struct lgdb_process_ctx *ctx);
void lgdb_handle_exit_thread_debug_event(struct lgdb_process_ctx *ctx);
void lgdb_handle_load_dll_debug_event(struct lgdb_process_ctx *ctx);
void lgdb_handle_unload_dll_debug_event(struct lgdb_process_ctx *ctx);
void lgdb_handle_output_debug_string_event(struct lgdb_process_ctx *ctx);

/* User-level debug events (that the user would care about) */
enum lgdb_user_event_type_t {
    /* The same as the system debug events */
    LUQT_EXCEPTION, // May be breakpoint if it is not user-defined (__debugbreak)
    LUQT_CREATE_PROCESS,
    LUQT_EXIT_PROCESS,
    LUQT_CREATE_THREAD,
    LUQT_EXIT_THREAD,
    LUQT_LOAD_DLL,
    LUQT_UNLOAD_DLL,
    LUQT_OUTPUT_DEBUG_STRING,

    /* Breakpoint for which there is actual source code (not in some system DLL) */
    LUQT_VALID_BREAKPOINT_HIT,
    /* Single step exception (may use this if stepping through dissassembly */
    LUQT_SINGLE_ASM_STEP,
    /* Single C/C++ source code step finished */
    LUQT_SINGLE_SOURCE_CODE_STEP_FINISHED,
    /* Step out of function finished */
    LUQT_STEP_OUT_FUNCTION_FINISHED,
    /* Step into function finished */
    LUQT_STEP_IN_FUNCTION_FINISHED
};

typedef struct lgdb_user_event {
    uint32_t ev_type;
    void *ev_data;
} lgdb_user_event_t;

/* 
    There are some debugger events / exceptions which require the user to do something 
    For instance, if a user breakpoint was hit, the debugger might want the user to step in/over/etc...
    There are events where the debugger does not require the user to do anything like certain
    single step exceptions, which the debugger knows how to handle itself etc...
    These events won't affect the user and don't have a lgdb_user_event_t associated with them.
*/
void lgdb_trigger_user_event(struct lgdb_process_ctx *ctx, uint32_t ev_type, void *ev_data);

#endif
