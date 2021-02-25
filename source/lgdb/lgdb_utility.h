#ifndef LGDB_UTILITY_H
#define LGDB_UTILITY_H

#include <stdint.h>
#include <Windows.h>

typedef uint32_t bool32_t;

#define LGDB_INVALID_HANDLE 0xFFFFFFF
typedef uint32_t lgdb_handle_t;

const char *lgdb_concat_cstr(const char *a, const char *b);
void lgdb_print_win32_error(const char *win32_function);
BOOL lgdb_check_win32_call(BOOL result, const char *func);
const char *lgdb_get_file_name_from_handle(HANDLE file);
/* Passed to certain Sym* functions */
BOOL lgdb_read_proc(HANDLE, DWORD64, PVOID, DWORD, LPDWORD);
uint32_t lgdb_hash_string(const char *string);
uint32_t lgdb_hash_buffer(const char *string, uint32_t length);
uint32_t lgdb_hash_pointer(void *p);
uint8_t lgdb_read_current_op_byte(struct lgdb_process_ctx *ctx, uint64_t pc, size_t *bytes_read);
const char *lgdb_get_base_type_string(uint32_t base_type);

#define WIN32_CALL(func, ...) \
    lgdb_check_win32_call(func(__VA_ARGS__), #func)

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#endif
