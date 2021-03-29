#include <Windows.h>
#include <stdio.h>
#include <stdint.h>

struct udt_t {
    uint32_t abba, babba, cabba;
    float fabba;
};

int main() {
    int *my_pointer = NULL;
    int *heap_pointer = (int *)malloc(sizeof(int) * 10);

    for (uint32_t i = 0; i < 10; ++i) {
        heap_pointer[i] = i * 2;
    }

    heap_pointer[0] = 42;

    printf("Hello debugger\n");

    udt_t *udt_pointer = (udt_t *)malloc(sizeof(udt_t) * 10);

    for (uint32_t i = 0; i < 10; ++i) {
        udt_pointer[i] = { i * 12, 42, (i + 12) / 2, 3.1415f * (float)(i +1) };
    }

    printf("Hello debugger again\n");

    return 0;
}
