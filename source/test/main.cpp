#include <Windows.h>
#include <stdio.h>

int global_variable = 12;

void foo() {
    for (int i = 0; i < 10; ++i) {
        printf("%d\n", i);
    }
}

int main() {
    printf("Entering main function!\n");

    if (global_variable > 9) {
        printf("Haha\n");
    }

    if (global_variable < 9) {
        printf("Hello\n");
    }

    for (int i = 0; i < 3; ++i) {
        printf("In loop %d\n", i);
    }

    __debugbreak();

    foo();

    foo();


    OutputDebugString("Hello debugger0\n");

    __debugbreak();

    return 0;
}