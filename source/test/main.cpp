#include <Windows.h>
#include <stdio.h>

int main() {
    OutputDebugString("Hello debugger\n");
    __debugbreak();

    return 0;
}