#include <Windows.h>
#include <stdio.h>

int global_variable = 12;




void foo() {
    OutputDebugString("In foo\n");
    __debugbreak();
}













int main() {
    printf("Entering main function!\n");

    foo();




    OutputDebugString("Hello debugger0\n");
    DebugBreak();

    return 0;
}