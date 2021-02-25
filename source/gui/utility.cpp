#include <stdio.h>
#include <stdlib.h>
#include "utility.hpp"


void error_and_exit(const char *msg) {
    fprintf(stderr, "ERROR: %s\n", msg);
    exit(-1);
}