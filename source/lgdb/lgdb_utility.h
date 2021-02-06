#ifndef LGDB_UTILITY_H
#define LGDB_UTILITY_H

#include <stdint.h>

typedef uint32_t bool32_t;

const char *lgdb_concat_cstr(const char *a, const char *b);
void lgdb_print_win32_error(const char *win32_function);
BOOL lgdb_check_win32_call(BOOL result, const char *func);
const char *lgdb_get_file_name_from_handle(HANDLE file);
/* Passed to certain Sym* functions */
BOOL lgdb_read_proc(HANDLE, DWORD64, PVOID, DWORD, LPDWORD);

#define WIN32_CALL(func, ...) \
    lgdb_check_win32_call(func(__VA_ARGS__), #func)

#endif
