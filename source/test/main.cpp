#include <Windows.h>
#include <stdio.h>

int global_variable = 12;

void foo() {
    OutputDebugString("In foo\n");
    // __debugbreak();
}

int main() {
    printf("Entering main function! %d\n", global_variable);
    foo();

    OutputDebugString("Hello debugger0\n");
    DebugBreak();

    OutputDebugString("Hello debugger1\n");
    DebugBreak();

    OutputDebugString("Hello debugger2\n");
    DebugBreak();

    OutputDebugString("Hello debugger3\n");
    DebugBreak();

    return 0;
}