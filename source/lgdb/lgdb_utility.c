#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <psapi.h>
#include <stdlib.h>
#include <stdio.h>
#include "lgdb_utility.h"
#include "lgdb_context.h"
#include <stdbool.h>

#include "TypeInfoStructs.h"


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
    printf("Call to \"%s\" failed with error %u: ", win32_function, last_error);
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

    char *str_file_name = malloc(sizeof(char) * BUFSIZE);

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
                            size_t name_len = strlen(name);

                            if (name_len < MAX_PATH) {
                                found = _strnicmp(file_name, name, name_len) == 0;

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
    *number_of_bytes_read = (DWORD)s;

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


uint8_t lgdb_read_current_op_byte(struct lgdb_process_ctx *ctx, uint64_t pc, size_t *bytes_read) {
    /* Made a jump to a different line - check for breakpoints */
    uint8_t op_byte;

    WIN32_CALL(
        ReadProcessMemory,
        ctx->proc_info.hProcess,
        (void *)ctx->thread_ctx.Rip,
        &op_byte,
        1,
        bytes_read);

    return op_byte;
}

/*

    btNoType = 0,
    btVoid = 1,
    btChar = 2,
    btWChar = 3,
    btInt = 6,
    btUInt = 7,
    btFloat = 8,
    btBCD = 9,
    btBool = 10,
    btLong = 13,
    btULong = 14,
    btCurrency = 25,
    btDate = 26,
    btVariant = 27,
    btComplex = 28,
    btBit = 29,
    btBSTR = 30,
    btHresult = 31
*/


const char *lgdb_get_base_type_string(uint32_t base_type) {
    switch (base_type) {
    case btNoType: return "";
    case btVoid: return "void";
    case btChar: return "char";
    case btWChar: return "wchar_t";
    case btInt: return "int";
    case btUInt: return "unsigned int";
    case btFloat: return "float";
    case btBCD: return "what is btBCD??";
    case btBool: return "bool";
    case btLong: return "long";
    case btULong: return "unsigned long";
    case btCurrency: return "what is btCurrency??";
    case btDate: return "what is btDate??";
    case btVariant: return "what is btVariant??";
    case btComplex: return "what is btComplex??";
    case btBit: return "unsigned int";
    case btBSTR: return "what is btBSTR??";
    case btHresult: return "HRESULT"; //??
    default: return "";
    }
}


void lgdb_read_buffer_from_process(struct lgdb_process_ctx *ctx, uint64_t ptr, uint32_t size, void *dst) {
    size_t bytes_read;
    WIN32_CALL(ReadProcessMemory,
        ctx->proc_info.hProcess,
        (void *)ptr,
        dst,
        size,
        &bytes_read);
}
