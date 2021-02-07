#include <Windows.h>
#include <psapi.h>
#include <stdlib.h>
#include <stdio.h>
#include "lgdb_utility.h"


const char *lgdb_concat_cstr(const char *a, const char *b) {
    size_t len_a = strlen(a), len_b = strlen(b);
    size_t len_concat = len_a + len_b;

    char *concat = (char *)malloc(sizeof(char) * (len_a + len_b + 1));

    memcpy_s(concat, len_a, a, len_a);
    memcpy_s(concat + len_a, len_b, b, len_b);
    concat[len_concat] = 0;

    return concat;
}


void lgdb_print_win32_error(const char *win32_function) {
    unsigned int last_error = GetLastError();
    printf("Call to \"%s\" failed with error %zu: ", win32_function, last_error);
    char buf[256] = { 0 };
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, last_error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        buf, (sizeof(buf) / sizeof(char)), NULL);
    printf(buf);
}


BOOL lgdb_check_win32_call(BOOL result, const char *func) {
    if (!result) {
        lgdb_print_win32_error(func);
    }

    return result;
}


#define BUFSIZE 512


const char *lgdb_get_file_name_from_handle(HANDLE file) {
    BOOL success = FALSE;
    char file_name[MAX_PATH + 1];
    HANDLE file_map;

    const char *str_file_name = malloc(sizeof(char) * BUFSIZE);

    // Get the file size.
    DWORD file_size_hi = 0;
    DWORD file_size_lo = GetFileSize(file, &file_size_hi);

    if (file_size_lo == 0 && file_size_hi == 0) {
        return FALSE;
    }

    // Create a file mapping object.
    file_map = CreateFileMapping(file,
        NULL,
        PAGE_READONLY,
        0,
        1,
        NULL);

    if (file_map) {
        // Create a file mapping to get the file name.
        void *mem = MapViewOfFile(file_map, FILE_MAP_READ, 0, 0, 1);

        if (mem) {
            if (GetMappedFileNameA(GetCurrentProcess(),
                mem,
                file_name,
                MAX_PATH)) {

                // Translate path with device name to drive letters.
                char temp[BUFSIZE];
                temp[0] = '\0';

                if (GetLogicalDriveStringsA(BUFSIZE - 1, temp)) {
                    char name[MAX_PATH];
                    char drive[3] = " :";
                    BOOL found = FALSE;
                    char *p = temp;

                    do {
                        // Copy the drive letter to the template string
                        *drive = *p;

                        // Look up each device name
                        if (QueryDosDeviceA(drive, name, MAX_PATH)) {
                            size_t name_len = _tcslen(name);

                            if (name_len < MAX_PATH) {
                                found = _tcsnicmp(file_name, name, name_len) == 0;

                                if (found) {
                                    sprintf(str_file_name, "%s%s", drive, file_name + name_len);
                                }
                            }
                        }

                        // Go to the next NULL character.
                        while (*p++);
                    } while (!found && *p); // end of string
                }
            }

            success = TRUE;
            UnmapViewOfFile(mem);
        }

        CloseHandle(file_map);

    }

    return(str_file_name);
}


BOOL lgdb_read_proc(
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


uint32_t lgdb_hash_string(const char *string) {
    unsigned long hash = 5381;
    uint32_t c;

    while (c = *string++) {
        hash = ((hash << 5) + hash) + c;
    }

    return hash;
}


uint32_t lgdb_hash_buffer(const char *string, uint32_t length) {
    unsigned long hash = 5381;

    uint32_t current = 0;

    while (current < length) {
        hash = ((hash << 5) + hash) + string[current];

        ++current;
    }

    return hash;
}


uint32_t lgdb_hash_pointer(void *p) {
    uint64_t u64 = (uint64_t)p;
    return (u64 & 0xffffffff) + (u64 >> 32);
}
