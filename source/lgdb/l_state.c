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
	TCHAR pszFilename[MAX_PATH+1];
	HANDLE hFileMap;

	const char *strFilename = malloc(sizeof(char) * BUFSIZE);

	// Get the file size.
	DWORD dwFileSizeHi = 0;
	DWORD dwFileSizeLo = GetFileSize(hFile, &dwFileSizeHi); 

	if( dwFileSizeLo == 0 && dwFileSizeHi == 0 )
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
		void* pMem = MapViewOfFile(hFileMap, FILE_MAP_READ, 0, 0, 1);

		if (pMem) 
		{
			if (GetMappedFileName (GetCurrentProcess(), 
				pMem, 
				pszFilename,
				MAX_PATH)) 
			{

				// Translate path with device name to drive letters.
				TCHAR szTemp[BUFSIZE];
				szTemp[0] = '\0';

				if (GetLogicalDriveStrings(BUFSIZE-1, szTemp)) 
				{
					TCHAR szName[MAX_PATH];
					TCHAR szDrive[3] = TEXT(" :");
					BOOL bFound = FALSE;
					TCHAR* p = szTemp;

					do 
					{
						// Copy the drive letter to the template string
						*szDrive = *p;

						// Look up each device name
						if (QueryDosDevice(szDrive, szName, MAX_PATH))
						{
							size_t uNameLen = _tcslen(szName);

							if (uNameLen < MAX_PATH) 
							{
								bFound = _tcsnicmp(pszFilename, szName, 
									uNameLen) == 0;

								if (bFound) 
								{
                                    sprintf(strFilename, "%s%s", szDrive, pszFilename+uNameLen);
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

const char *concat_str(const char *a, const char *b) {
    size_t len_a = strlen(a), len_b = strlen(b);
    size_t len_concat = len_a + len_b;

    char *concat = (char *)malloc(sizeof(char) * (len_a + len_b + 1));

    memcpy_s(concat, len_a, a, len_a);
    memcpy_s(concat + len_a, len_b, b, len_b);
    concat[len_concat] = 0;

    return concat;
}

static BOOL s_read_proc(HANDLE process, DWORD64 base_addr, PVOID buffer, DWORD size, LPDWORD number_of_bytes_read) {
    SIZE_T s;
    BOOL success = ReadProcessMemory(process, (LPVOID)base_addr, buffer, size, &s);
    *number_of_bytes_read = s;

    return success;
}


/* For now, just store here whilst we are still experimenting */
static DWORD64 process_pdb_base;
static IMAGEHLP_MODULE64 module_info;


static void s_process_exception_code(const LPDEBUG_EVENT debug_ev, HANDLE process, HANDLE thread_id) {
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

        BOOL success = GetThreadContext(thread_id, &ctx);

        if (!success) {
            wchar_t buf[256];
            FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                buf, (sizeof(buf) / sizeof(wchar_t)), NULL);
            wprintf(buf);
        }

        IMAGEHLP_LINE64 line;
        line.SizeOfStruct = sizeof(line);
        DWORD displacement;
        success = SymGetLineFromAddr64(process, (DWORD)ctx.Rip, &displacement, &line);

        if (success) {
            printf("Line %d\n", line.LineNumber);
        }

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
            success = StackWalk64(
                IMAGE_FILE_MACHINE_I386,
                process,
                thread_id,
                &stack,
                &ctx,
                s_read_proc,
                SymFunctionTableAccess64,
                SymGetModuleBase64, 0);

            // stack.AddrPC.Offset = ctx.Rip;

            IMAGEHLP_MODULE64 module = { 0 };
            module.SizeOfStruct = sizeof(module);
            success = SymGetModuleInfo64(process, (DWORD64)stack.AddrPC.Offset, &module);

            if (!success) {
                wchar_t buf[256];
                FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    buf, (sizeof(buf) / sizeof(wchar_t)), NULL);
                wprintf(buf);
            }

            // Read information in the stack structure and map to symbol file
            DWORD displacement;
            IMAGEHLP_SYMBOL64 *symbol = (IMAGEHLP_SYMBOL64 *)malloc(sizeof(IMAGEHLP_SYMBOL64) + MAX_SYM_NAME);

            memset(symbol, 0, sizeof(IMAGEHLP_SYMBOL64) + MAX_SYM_NAME);
            symbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
            symbol->MaxNameLength = MAX_SYM_NAME;

            success = SymGetSymFromAddr64(process, stack.AddrPC.Offset, &displacement, symbol);

            if (!success) {
                wchar_t buf[256];
                FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    buf, (sizeof(buf) / sizeof(wchar_t)), NULL);
                wprintf(buf);
            }
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

static void s_debug_loop(HANDLE process, HANDLE thread_id, const char *exe_name) {
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
                s_process_exception_code(&debug_ev, process, thread_id);
            } break;

            case CREATE_PROCESS_DEBUG_EVENT: {
                process_pdb_base = SymLoadModule64(
                    process,
                    debug_ev.u.CreateProcessInfo.hFile,
                    exe_name,
                    0,
                    debug_ev.u.CreateProcessInfo.lpBaseOfImage,
                    0);

                // Determine if the symbols are available
                module_info.SizeOfStruct = sizeof(module_info);
                BOOL success = SymGetModuleInfo64(
                    process,
                    process_pdb_base,
                    &module_info);

                if (success && module_info.SymType == SymPdb) {
                    printf("Symbols were properly loaded\n");
                }
                else {
                    printf("Symbols were not properly loaded\n");

                    wchar_t buf[256];
                    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                        NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                        buf, (sizeof(buf) / sizeof(wchar_t)), NULL);

                    wprintf(buf);
                }

                HANDLE main_thread = debug_ev.u.CreateProcessInfo.hThread;

                CONTEXT ctx;
                success = GetThreadContext(main_thread, &ctx);

                if (!success) {
                    printf("Somethin went wrong\n");
                }

                // s_set_breakpoint_at(debug_ev.u.CreateProcessInfo.lpStartAddress);
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

                DWORD64 dwBase = SymLoadModule64(process, debug_ev.u.LoadDll.hFile, dll_name,
                    0, (DWORD64)debug_ev.u.LoadDll.lpBaseOfDll, 0);

                if (!dwBase) {
                    printf("Symbols were not properly loaded\n");

                    wchar_t buf[256];
                    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                        NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                        buf, (sizeof(buf) / sizeof(wchar_t)), NULL);

                    wprintf(buf);
                }

                printf("Loaded DLL: %s\n", dll_name);

                // Code continues from above
                IMAGEHLP_MODULE64 module;
                module.SizeOfStruct = sizeof(module);
                BOOL success = SymGetModuleInfo64(process, dwBase, &module);

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

        wprintf(buf);
    }

    success = SymInitialize(process_info.hProcess, NULL, FALSE);
    // To intidivually load symbols, use SymLoadModule64 (Needs to be in CREATE_PROCESS_DEBUG_EVENT)

    s_debug_loop(process_info.hProcess, process_info.hThread, exe_path);

    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);

    free((void *)exe_path);
}

void lgdb_test() {

    s_start_process("C:\\Users\\lucro\\Development\\lgdb\\build\\Debug\\", "lgdbtest.exe");
}
