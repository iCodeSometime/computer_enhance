//
// Created by Kenneth Cochran on 10/29/26.
//

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "util/preamble.h"
#include "util/virtual_arena/virtual_arena.h"

#include "../instruction_set/instruction_set.h"
#include "util/_tests/tests.h"


static void test_single_file(const char* file_path, instruction_allocators* allocators, u8 expected_instruction_count, instruction* expected_instructions) {
    bool should_clean_allocators = false;
    if (allocators == NULL) {
        should_clean_allocators = true;
        virtual_arena* general_allocator = create_arena(1000000000ULL);
        allocators = allocate_bytes(general_allocator, sizeof(instruction_allocators), 16);
        allocators->general_allocator = general_allocator;
        allocators->bitpattern_allocator = create_arena(1000000000ULL);
        allocators->instruction_allocator = create_arena(1000000000ULL);
    }

    instruction* instructions = allocate_bytes(allocators->instruction_allocator, 0, 1);
    u8 instruction_count = parse_instructions(file_path, *allocators);

    EXPECT(instruction_count == expected_instruction_count);

    for (u8 i = 0; i < instruction_count; i++) {
        EXPECT(strcmp(instructions[i].name, expected_instructions[i].name) == 0);
        EXPECT(strcmp(instructions[i].mnemonic, expected_instructions[i].mnemonic) == 0);
        EXPECT(strcmp(instructions[i].short_description, expected_instructions[i].short_description) == 0);
        EXPECT(strcmp(instructions[i].description, expected_instructions[i].description) == 0);
        EXPECT(strcmp(instructions[i].variant_name, expected_instructions[i].variant_name) == 0);

        // bits are not normal strings
        for (u8 j = 0; j < 32; j++) {
            EXPECT(instructions[i].bits[j] == expected_instructions[i].bits[j]);
        }
    }

    if (should_clean_allocators) {
        destroy_arena(allocators->bitpattern_allocator);
        destroy_arena(allocators->instruction_allocator);
        destroy_arena(allocators->general_allocator); // Must be last since allocators is stored here.
        allocators = NULL;
    }
}

static void test_add_immediate(void) {
    instruction instruction = {
        .name = "ADD (immediate)",
        .mnemonic = "ADD",
        .short_description = "Add immediate value",
        .description = "This instruction adds a register value and an optionally-shifted\nimmediate value, and writes the result to the destination register.\n",
        .variant_name = "ADD",
        .bits = (char[32]){"xxxxxxxxxxxxxxxxxxxxxxx01000100x"},
    };

    test_single_file("add_addsub_imm.xml", NULL, 1, &instruction);
}

int run_instruction_set_parser_tests(void) {
    printf("-- parse add_addsub_imm.xml\n");
    test_add_immediate();
}
