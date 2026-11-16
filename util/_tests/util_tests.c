//
// Created by Kenneth Cochran on 10/20/26.
//

#include "virtual_arena_tests.c"

void run_util_tests(void) {
    run_virtual_arena_tests();
}

#ifndef AGGREGATE_TESTS
int main(void) {
    run_util_tests();
}
#endif
