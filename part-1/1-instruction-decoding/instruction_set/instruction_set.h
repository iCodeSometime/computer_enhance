//
// Created by Kenneth Cochran on 10/29/26.
//

#ifndef INSTRUCTION_SET_H
#define INSTRUCTION_SET_H

// todo: is it useful to have sections here?
typedef struct instruction {
    char* name;
    char* mnemonic;
    char* short_description;
    char* description;
    char* variant_name;
    // 0, 1, or x -- always 32
    char* bits;
} instruction;

typedef struct instruction_allocators {
    virtual_arena* general_allocator;
    virtual_arena* instruction_allocator;
    virtual_arena* bitpattern_allocator;
} instruction_allocators;

int parse_instructions(const char* file_name, const instruction_allocators allocators);

#endif //INSTRUCTION_SET_H
