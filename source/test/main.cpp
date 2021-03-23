#include <Windows.h>
#include <stdio.h>
#include <stdint.h>

int main() {
    int *my_pointer = NULL;
    int *heap_pointer = (int *)malloc(sizeof(int) * 10);

    for (uint32_t i = 0; i < 10; ++i) {
        heap_pointer[i] = i * 2;
    }

    heap_pointer[0] = 42;

    printf("Hello debugger\n");
    return 0;
}
