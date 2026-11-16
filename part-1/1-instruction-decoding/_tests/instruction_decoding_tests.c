//
// Created by Kenneth Cochran on 10/29/26.
//

#include "instruction_set_parser_tests.c"
#include "generate_decision_tree_tests.c"

void run_instruction_decoding_tests(void) {
    run_instruction_set_parser_tests();
    run_generate_decision_tree_tests();
}

#ifndef AGGREGATE_TESTS
int main(void) {
    run_instruction_decoding_tests();
}
#endif
