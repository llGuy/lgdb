#include <Windows.h>
#include <stdio.h>

int global_variable = 12;

__forceinline void foo(int a, int b, int c) {
    for (int i = 0; i < 10; ++i) {
        printf("%d %d %d %d\n", i, a, b, c);
    }
}

int main() {
    int first_variable = 42;
    float second_variable = 3.1415f;

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

    foo(global_variable, 2, global_variable + 1);

    foo(4, 2, global_variable - 1);


    OutputDebugString("Hello debugger0\n");

    __debugbreak();

    return 0;
}