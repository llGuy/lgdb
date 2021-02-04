#include <Windows.h>
#include <stdio.h>

int main() {
    printf("Entering main function!\n");

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