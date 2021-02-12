#include <Windows.h>
#include <stdio.h>

wchar_t global_variable = 12;
int global_variable0 = 12;
float global_variable1 = 12.0f;
int global_variable2 = 12;

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
        __debugbreak();
        printf("Haha\n");
    }

    if (global_variable < 9) {
        printf("Hello\n");
    }

    for (int i = 0; i < 3; ++i) {
        printf("In loop %d\n", i);
    }

    foo(global_variable, 2, global_variable + 1);

    foo(4, 2, global_variable - 1);


    OutputDebugString("Hello debugger0\n");

    __debugbreak();

    return 0;
}