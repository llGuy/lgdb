#include <Windows.h>
#include <stdio.h>

int global_variable = 12;




void foo() {
    OutputDebugString("In foo\n");
}













int main() {
    printf("Entering main function!\n");

    foo();

    foo();

    foo();


    OutputDebugString("Hello debugger0\n");

    DebugBreak();

    return 0;
}