#include <Windows.h>
#include <stdio.h>
#include <stdint.h>

wchar_t global_variable = 12;
int global_variable0[10] = { 0 };
float global_variable1 = 12.0f;
int global_variable2[10] = { 0 };

static int static_variable = 12;

__forceinline void foo(int a, int b, int c) {
    for (int i = 0; i < 10; ++i) {
        printf("%d %d %d %d\n", i, a, b, c);
    }
}

struct test_base_class0_t {
    int base_int0;
    float base_float0;
};

struct test_base_class1_t {
    int base_int1;
    float base_float1;
};

struct test_derived_class_t : test_base_class0_t, test_base_class1_t {
    int derived_int;
    float derived_float;
};

struct a_basic_structure_t {
    int a;
    float b;
    char some_array[5];
};

enum my_enum_t {
    ME_AM_AWESOME = 12,
    ME_AM_SO_COOL = 13
};

void some_function(int a, int b, char *pointer) {
    printf("%d %d %p\n", a, b, pointer);
}

extern void lets_go_to_another_file();

int assembly_test(int first_arg, int second_arg) {
    char buffer[] = "Here we are!";

    int a = first_arg + 1;
    int b = second_arg - 1;

    return a * b;
}


int main() {
    int i = 12;

    int b = i * assembly_test(i + 1, i - 1);

    printf("%d\n", b);



#if 0
    int a_variable = 12;
    a_basic_structure_t stru = {};
    my_enum_t en = ME_AM_SO_COOL;

    {
        int i = 0;
        printf("%d\n", i);
    }

    int i = 12;
    printf("%d\n", i);

    int ret = assembly_test(5, 9);

    lets_go_to_another_file();

    printf("Hello Luc's Debugger!\n");

    my_enum_t my_thingy = ME_AM_AWESOME;

    void (*function_pointer)(int, int, char *) = some_function;


    test_derived_class_t derived = {};
    derived.derived_int = 12;
    derived.derived_float = 3.0f;
    derived.base_int0 = 1;
    derived.base_float0 = 5.0f;

    a_basic_structure_t my_structure = {};
    my_structure.a = 12;
    my_structure.b = 3.14f;
    my_structure.some_array[0] = 'b';
    my_structure.some_array[1] = 'e';
    my_structure.some_array[2] = 'e';
    my_structure.some_array[3] = 'f';
    my_structure.some_array[4] = 0;

    int base_type = 12;
    uint32_t typedefed_type = 13;

    unsigned int an_array[3] = { 42, 69, 0xDEADBEEF };

    int first_variable = 42;
    float second_variable = 3.1415f;

    printf("Entering main function!\n");

    first_variable += 12;
    second_variable /= 2.0f;

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
#endif


    return 0;
}
