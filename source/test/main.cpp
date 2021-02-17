#include <Windows.h>
#include <stdio.h>
#include <stdint.h>

wchar_t global_variable = 12;
int global_variable0[10] = { 0 };
float global_variable1 = 12.0f;
int global_variable2[10] = { 0 };

__forceinline void foo(int a, int b, int c) {
    for (int i = 0; i < 10; ++i) {
        printf("%d %d %d %d\n", i, a, b, c);
    }
}

struct a_basic_structure_t {
    int a;
    float b;
    char some_array[5];
};

int main() {
    a_basic_structure_t my_structure = {};
    my_structure.a = 12;
    my_structure.b = 3.14f;
    my_structure.some_array[0] = 'b';
    my_structure.some_array[1] = 'e';
    my_structure.some_array[2] = 'e';
    my_structure.some_array[3] = 'f';
    my_structure.some_array[4] = 0;

#if 0
    int base_type = 12;
    uint32_t typedefed_type = 13;

    unsigned int an_array[3] = { 42, 69, 0xDEADBEEF };

    int first_variable = 42;
    float second_variable = 3.1415f;

    printf("Entering main function!\n");

    first_variable += 12;
    second_variable /= 2.0f;
#endif

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