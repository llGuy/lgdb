#include <Windows.h>
#include <stdio.h>
#include <stdint.h>

struct base_t {
    int base_int;
};

struct udt_t : base_t {
    uint32_t abba, babba, cabba;
    float fabba;

    uint32_t *ptr;
    udt_t *next;
};

int main() {
    int *my_pointer = NULL;
    int *heap_pointer = (int *)malloc(sizeof(int) * 10);

    for (uint32_t i = 0; i < 10; ++i) {
        heap_pointer[i] = i * 2;
    }

    heap_pointer[0] = 42;

    printf("Hello debugger\n");

    udt_t udt_on_stack = {};

    udt_t *udt_pointer = (udt_t *)malloc(sizeof(udt_t) * 10);
    memset(udt_pointer, 0, sizeof(udt_t) * 10);

    for (uint32_t i = 0; i < 10; ++i) {
        udt_pointer[i].base_int = i * 12;
        udt_pointer[i].abba = i * 12;
        udt_pointer[i].babba = i * 12;
        udt_pointer[i].cabba = i * 12;
        udt_pointer[i].fabba = 3.14f * (float)(i + 1);
        udt_pointer[i].ptr = new uint32_t(i * 12);
        udt_pointer[i].next = &udt_pointer[i + 1];
        printf("%p\n", udt_pointer[i].ptr);
    }

    printf("Hello debugger again\n");

    return 0;
}
