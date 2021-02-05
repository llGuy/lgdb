#define _CRT_SECURE_NO_WARNINGS

// MapViewOfFile
#include <stdio.h>
#include "l_state.h"

#include <Windows.h>
#include <DbgHelp.h>
#include <psapi.h>

/* This function isn't my code */
const char *GetFileNameFromHandle(HANDLE hFile)
{
#define BUFSIZE 512

    BOOL bSuccess = FALSE;
    char pszFilename[MAX_PATH + 1];
    HANDLE hFileMap;

    const char *strFilename = malloc(sizeof(char) * BUFSIZE);

    // Get the file size.
    DWORD dwFileSizeHi = 0;
    DWORD dwFileSizeLo = GetFileSize(hFile, &dwFileSizeHi);

    if (dwFileSizeLo == 0 && dwFileSizeHi == 0)
    {
        return FALSE;
    }

    // Create a file mapping object.
    hFileMap = CreateFileMapping(hFile,
        NULL,
        PAGE_READONLY,
        0,
        1,
        NULL);

    if (hFileMap)
    {
        // Create a file mapping to get the file name.
        void *pMem = MapViewOfFile(hFileMap, FILE_MAP_READ, 0, 0, 1);

        if (pMem)
        {
            if (GetMappedFileNameA(GetCurrentProcess(),
                pMem,
                pszFilename,
                MAX_PATH))
            {

                // Translate path with device name to drive letters.
                char szTemp[BUFSIZE];
                szTemp[0] = '\0';

                if (GetLogicalDriveStringsA(BUFSIZE - 1, szTemp))
                {
                    char szName[MAX_PATH];
                    char szDrive[3] = " :";
                    BOOL bFound = FALSE;
                    char *p = szTemp;

                    do
                    {
                        // Copy the drive letter to the template string
                        *szDrive = *p;

                        // Look up each device name
                        if (QueryDosDeviceA(szDrive, szName, MAX_PATH))
                        {
                            size_t uNameLen = _tcslen(szName);

                            if (uNameLen < MAX_PATH)
                            {
                                bFound = _tcsnicmp(pszFilename, szName,
                                    uNameLen) == 0;

                                if (bFound)
                                {
                                    sprintf(strFilename, "%s%s", szDrive, pszFilename + uNameLen);
                                }
                            }
                        }

                        // Go to the next NULL character.
                        while (*p++);
                    } while (!bFound && *p); // end of string
                }
            }
            bSuccess = TRUE;
            UnmapViewOfFile(pMem);
        }

        CloseHandle(hFileMap);

    }

    return(strFilename);
}

typedef struct {

    /* Process handle, main thread, etc... */
    PROCESS_INFORMATION proc_info;
    /* For now, just store here whilst we are still experimenting */
    DWORD64 process_pdb_base;
    IMAGEHLP_MODULE64 module_info;

    const char *exe_path;

} lgdb_process_ctx_t;

static void s_print_win32_error(const char *win32_function) {
    unsigned int last_error = GetLastError();
    printf("Call to \"%s\" failed with error %zu: ", win32_function, last_error);
    char buf[256] = { 0 };
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, last_error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        buf, (sizeof(buf) / sizeof(char)), NULL);
    printf(buf);
}

static inline BOOL s_check_win32_call(BOOL result, const char *func) {
    if (!result) {
        s_print_win32_error(func);
    }

    return result;
}

#define WIN32_CALL(func, ...) \
    s_check_win32_call(func(__VA_ARGS__), #func)

#if 0
#define WIN32_CALL(func, ...) \
    if (!(func(__VA_ARGS__))) s_print_win32_error(#func)
#endif

const char *concat_str(const char *a, const char *b) {
    size_t len_a = strlen(a), len_b = strlen(b);
    size_t len_concat = len_a + len_b;

    char *concat = (char *)malloc(sizeof(char) * (len_a + len_b + 1));

    memcpy_s(concat, len_a, a, len_a);
    memcpy_s(concat + len_a, len_b, b, len_b);
    concat[len_concat] = 0;

    return concat;
}

static BOOL s_read_proc(
    HANDLE process,
    DWORD64 base_addr,
    PVOID buffer,
    DWORD size,
    LPDWORD number_of_bytes_read) {
    SIZE_T s;
    BOOL success = ReadProcessMemory(process, (LPVOID)base_addr, buffer, size, &s);
    *number_of_bytes_read = s;

    return success;
}

static void s_process_exception_code(const LPDEBUG_EVENT debug_ev, lgdb_process_ctx_t *proc_ctx) {
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
        printf("Code breakpoint hit!\n");

        CONTEXT ctx = {
            .ContextFlags = CONTEXT_FULL
        };

        BOOL success = WIN32_CALL(
            GetThreadContext,
            proc_ctx->proc_info.hThread,
            &ctx);

        STACKFRAME64 stack = {
            .AddrPC.Offset = ctx.Rip,
            .AddrPC.Mode = AddrModeFlat,
            .AddrFrame.Offset = ctx.Rbp,
            .AddrFrame.Mode = AddrModeFlat,
            .AddrStack.Offset = ctx.Rsp,
            .AddrStack.Mode = AddrModeFlat
        };

        HANDLE compare = debug_ev->dwProcessId;

        do {
            success = WIN32_CALL(StackWalk64,
                IMAGE_FILE_MACHINE_AMD64,
                proc_ctx->proc_info.hProcess,
                proc_ctx->proc_info.hThread,
                &stack,
                &ctx,
                s_read_proc,
                SymFunctionTableAccess64,
                SymGetModuleBase64, 0);

            IMAGEHLP_MODULE64 module = { 0 };
            module.SizeOfStruct = sizeof(module);
            WIN32_CALL(
                SymGetModuleInfo64,
                proc_ctx->proc_info.hProcess,
                (DWORD64)stack.AddrPC.Offset,
                &module);

            // Read information in the stack structure and map to symbol file
            DWORD64 displacement;
            IMAGEHLP_SYMBOL64 *symbol = (IMAGEHLP_SYMBOL64 *)malloc(sizeof(IMAGEHLP_SYMBOL64) + MAX_SYM_NAME);

            memset(symbol, 0, sizeof(IMAGEHLP_SYMBOL64) + MAX_SYM_NAME);
            symbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
            symbol->MaxNameLength = MAX_SYM_NAME;

            WIN32_CALL(
                SymGetSymFromAddr64,
                proc_ctx->proc_info.hProcess,
                stack.AddrPC.Offset,
                &displacement,
                symbol);

            printf("%s\n", symbol->Name);

        } while (stack.AddrReturn.Offset != 0);
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

static BOOL s_sym_enum_proc(
    PSYMBOL_INFO sym_info,
    ULONG size,
    PVOID user_ctx) {
    printf("Got symbol: %s\n", sym_info->Name);
    return 1;
}

static void s_debug_loop(lgdb_process_ctx_t *proc_ctx) {
    DWORD continue_status = DBG_CONTINUE;
    DEBUG_EVENT debug_ev;

    for (;;) {
        // TODO: Figure out what the difference between process and debug_ev.u.CreateProcessInfo.hProcess is

        /* Wait for debugger event to occur */
        // TODO: Replace INFINITE with 0 so that we can easily check for events in GUI app
        BOOL success = WaitForDebugEvent(&debug_ev, INFINITE);

        if (success) {
            switch (debug_ev.dwDebugEventCode) {

            case EXCEPTION_DEBUG_EVENT: {
                s_process_exception_code(&debug_ev, proc_ctx);
            } break;

            case CREATE_PROCESS_DEBUG_EVENT: {
                WIN32_CALL(SymInitialize, proc_ctx->proc_info.hProcess, NULL, FALSE);

                proc_ctx->process_pdb_base = SymLoadModule64(
                    proc_ctx->proc_info.hProcess,
                    debug_ev.u.CreateProcessInfo.hFile,
                    proc_ctx->exe_path,
                    0,
                    debug_ev.u.CreateProcessInfo.lpBaseOfImage,
                    0);

                // success = WIN32_CALL(SymEnumSymbols, proc_ctx->proc_info.hProcess, proc_ctx->process_pdb_base, "", s_sym_enum_proc, NULL);

                // Determine if the symbols are available
                proc_ctx->module_info.SizeOfStruct = sizeof(proc_ctx->module_info);
                success = WIN32_CALL(SymGetModuleInfo64,
                    proc_ctx->proc_info.hProcess,
                    proc_ctx->process_pdb_base,
                    &proc_ctx->module_info);

                if (success && proc_ctx->module_info.SymType == SymPdb) {
                    printf("Symbols were properly loaded\n");
                }
                else {
                    printf("Symbols were not properly loaded\n");
                }
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
                const char *dll_name = GetFileNameFromHandle(debug_ev.u.LoadDll.hFile);

                DWORD64 base = SymLoadModule64(
                    proc_ctx->proc_info.hProcess,
                    debug_ev.u.LoadDll.hFile,
                    dll_name,
                    0,
                    (DWORD64)debug_ev.u.LoadDll.lpBaseOfDll,
                    0);

                if (!base) {
                    printf("Symbols were not properly loaded\n");

                    s_print_win32_error("SymLoadModule64");
                }

                printf("Loaded DLL: %s\n", dll_name);

                // Code continues from above
                IMAGEHLP_MODULE64 module;
                module.SizeOfStruct = sizeof(module);
                BOOL success = WIN32_CALL(SymGetModuleInfo64, proc_ctx->proc_info.hProcess, base, &module);

                // Check and notify
                if (success && module.SymType == SymPdb) {
                    printf("Succeded in loading DLL symbols\n");
                }
                else {
                    printf("Failed in loading DLL symbols\n");
                }
            } break;

            case UNLOAD_DLL_DEBUG_EVENT: {
            } break;

            case OUTPUT_DEBUG_STRING_EVENT: {
                printf("Sent some string to the debugger!\n");

                debug_ev.u.DebugString.lpDebugStringData;
                debug_ev.u.DebugString.nDebugStringLength;

                void *dst_ptr = malloc(debug_ev.u.DebugString.nDebugStringLength * sizeof(char));
                size_t bytes_read = 0;

                BOOL success = WIN32_CALL(ReadProcessMemory,
                    proc_ctx->proc_info.hProcess,
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
        else {
            printf("Didn't receive debug event\n");
        }
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

    lgdb_process_ctx_t *ctx = (lgdb_process_ctx_t *)malloc(sizeof(lgdb_process_ctx_t));
    ctx->exe_path = exe_path;

    WIN32_CALL(CreateProcessA,
        exe_path,
        NULL, // If parameters are to be passed, write the cmdline command here
        NULL,
        NULL,
        FALSE,
        DEBUG_ONLY_THIS_PROCESS,
        NULL,
        directory,
        &startup_info,
        &ctx->proc_info);

    // To intidivually load symbols, use SymLoadModule64 (Needs to be in CREATE_PROCESS_DEBUG_EVENT)

    s_debug_loop(ctx);

    CloseHandle(ctx->proc_info.hThread);
    CloseHandle(ctx->proc_info.hProcess);

    free((void *)exe_path);
}

void lgdb_test() {

    s_start_process("C:\\Users\\lucro\\Development\\debugger\\lGdb\\lGdb\\x64\\Debug\\", "lGdb-test.exe");
}
