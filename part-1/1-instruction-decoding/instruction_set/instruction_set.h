//
// Created by Kenneth Cochran on 10/29/26.
//

#ifndef INSTRUCTION_SET_H
#define INSTRUCTION_SET_H

#include "util/virtual_arena/virtual_arena.h"
#define XML_PATH(path) "../part-1/1-instruction-decoding/instruction_set/data/ISA_A64/" path

// todo: is it useful to have sections here?
typedef struct instruction {
    char* name;
    char* mnemonic;
    char* short_description;
    char* description;
    char* variant_name;
    // 0, 1, or x
    char bits[32];
} instruction;

typedef struct instruction_allocators {
    virtual_arena* general_allocator;
    virtual_arena* instruction_allocator;
    virtual_arena* bitpattern_allocator;
} instruction_allocators;

int parse_instructions(const char* file_path, const instruction_allocators allocators);

// Brace-initializer for char[32]. Pattern MSB-first → bits[31]; length checked via sizeof.
#define INSTRUCTION_LITERAL(x) \
    { \
        (char)(0 * sizeof(char[sizeof(#x) == 33 ? 1 : -1]) + #x[31]), \
        #x[30], #x[29], #x[28], #x[27], #x[26], #x[25], #x[24], \
        #x[23], #x[22], #x[21], #x[20], #x[19], #x[18], #x[17], #x[16], \
        #x[15], #x[14], #x[13], #x[12], #x[11], #x[10], #x[9],  #x[8],  \
        #x[7],  #x[6],  #x[5],  #x[4],  #x[3],  #x[2],  #x[1],  #x[0]   \
    }

#endif //INSTRUCTION_SET_H
