#include <stdio.h>
#include "l_state.h"

#include <windows.h>

const char *concat_str(const char *a, const char *b) {
    size_t len_a = strlen(a), len_b = strlen(b);
    size_t len_concat = len_a + len_b;

    char *concat = (char *)malloc(sizeof(char) * (len_a + len_b + 1));

    memcpy_s(concat, len_a, a, len_a);
    memcpy_s(concat + len_a, len_b, b, len_b);
    concat[len_concat] = 0;

    return concat;
}

static void s_process_exception_code(const LPDEBUG_EVENT debug_ev) {
    /*
        Process the exception code. When handling exceptions,
        remember to set the continuation status parameter
        This value is used by the ContinueDebugEvent function
    */
    switch (debug_ev->u.Exception.ExceptionRecord.ExceptionCode) {
    case EXCEPTION_ACCESS_VIOLATION: {
        /*
            First change: pass this on to the system
            Last chance: display an appropriate error
        */
    } break;

    case EXCEPTION_BREAKPOINT: {
        /*
            First change: display the current instruction
            and register values.
        */
    } break;

    case EXCEPTION_DATATYPE_MISALIGNMENT: {
        /*
            First change: pass this on to the system
            Last change: display an appropriate error
        */
    } break;

    case EXCEPTION_SINGLE_STEP: {
        /*
            First chance: pass this on to the system
            Last change: display an appropriate error
        */
    } break;

    case DBG_CONTROL_C: {
        /*
            First chance: pass this on to the system
            Last chance: display an appropriate error
        */
    } break;

    default: {
        /* Otherwise */
    } break;

    }
}

static void s_debug_loop(HANDLE process) {
    DWORD continue_status = DBG_CONTINUE;
    DEBUG_EVENT debug_ev;

    for (;;) {
        /* Wait for debugger event to occur */
        WaitForDebugEvent(&debug_ev, INFINITE);

        switch (debug_ev.dwDebugEventCode) {

        case EXCEPTION_DEBUG_EVENT: {
            s_process_exception_code(&debug_ev);
        } break;

        case CREATE_THREAD_DEBUG_EVENT: {
            /*
                As needed, examine or change the thread's registers
                with the GetThreadContext and SetThreadContext functions;
                and suspend and resume thread execution with the 
                SuspendThread and ResumeThread functions
            */
        } break;

        case EXIT_THREAD_DEBUG_EVENT: {
        } break;

        case EXIT_PROCESS_DEBUG_EVENT: {
        } break;

        case LOAD_DLL_DEBUG_EVENT: {
        } break;

        case UNLOAD_DLL_DEBUG_EVENT: {
        } break;

        case OUTPUT_DEBUG_STRING_EVENT: {
            printf("Sent some string to the debugger!\n");

            debug_ev.u.DebugString.lpDebugStringData;
            debug_ev.u.DebugString.nDebugStringLength;

            void *dst_ptr = malloc(debug_ev.u.DebugString.nDebugStringLength * sizeof(char));
            size_t bytes_read = 0;

            BOOL success = ReadProcessMemory(
                process,
                debug_ev.u.DebugString.lpDebugStringData,
                dst_ptr,
                debug_ev.u.DebugString.nDebugStringLength,
                &bytes_read);

            printf((char *)dst_ptr);

            free(dst_ptr);
        } break;

        }

        ContinueDebugEvent(debug_ev.dwProcessId, debug_ev.dwThreadId, continue_status);
    }
}

static void s_start_process(const char *directory, const char *executable_name) {
    const char *exe_path = concat_str(directory, executable_name);

    // Most of the fields here are not needed unless we set it in dwFlags
    STARTUPINFO startup_info = {
        .cb = sizeof(STARTUPINFO),
        .lpReserved = NULL,
        .lpDesktop = NULL,
        .lpTitle = executable_name,
        .dwX = 0, // Perhaps do something by taking the display rect
        .dwY = 0,
        .dwXSize = 0,
        .dwYSize = 0,
        .dwXCountChars = 0,
        .dwYCountChars = 0,
        .dwFlags = 0, // May need to look into this later
        .wShowWindow = 0,
        .cbReserved2 = 0,
        .lpReserved2 = NULL,
        .hStdInput = NULL,
        .hStdError = NULL
    };

    // Make sure to close process_info.hProcess and process.hThread with CloseHandle()
    PROCESS_INFORMATION process_info;

    BOOL success = CreateProcessA(
        exe_path,
        NULL, // If parameters are to be passed, write the cmdline command here
        NULL,
        NULL,
        FALSE,
        DEBUG_ONLY_THIS_PROCESS,
        NULL,
        directory,
        &startup_info,
        &process_info);

    if (!success) {
        wchar_t buf[256];
        FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            buf, (sizeof(buf) / sizeof(wchar_t)), NULL);

        printf(buf);
    }

    s_debug_loop(process_info.hProcess);

    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);

    free((void *)exe_path);
}

void lgdb_test() {
    s_start_process("C:\\Users\\lucro\\Development\\lgdb\\build\\Debug\\", "lgdbtest.exe");
}
