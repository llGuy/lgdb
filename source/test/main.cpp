#include <Windows.h>
#include <stdio.h>

int global_variable = 12;




void foo() {
    OutputDebugString("In foo\n");
}













int main() {
    printf("Entering main function!\n");

    if (global_variable < 9) {
        printf("Haha\n");
    }

    foo();

    __debugbreak();

    foo();

    foo();


    OutputDebugString("Hello debugger0\n");

    __debugbreak();

    return 0;
}