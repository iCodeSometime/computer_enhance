//
// Created by Kenneth Cochran on 10/29/26.
//

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>


#include "util/virtual_arena/virtual_arena.h"

#include "instruction_set.h"
#include "util/file_loader/file_loader.h"

#define SKIP_UNTIL_AFTER(data, pattern, end) \
    ({ \
        char* _at = skip_until((data), (pattern), (end)); \
        _at == (end) ? _at : _at + sizeof(pattern) - 1; \
    })

static int is_match(const char* data, const char* pattern, const char* end);
static char* skip_until(char* data, const char* pattern, const char* end);
static char* skip_whitespace(char* data, const char* end);

static char* parse_description(char* data, instruction* instruction, const instruction_allocators allocators, const char* end);
static char* parse_classes(char* data, instruction* instruction, const instruction_allocators allocators, const char* end, int* parsedCount);
static char* parse_regdiagram(char* data, instruction* instruction, const char* end);
static char* parse_encoding(char* data, instruction* instruction, instruction_allocators allocators, const char* end);
static char* parse_bitpattern(char* data, char* bits, const char* end);

/// Parse the instruction set XML that ARM publishes.
/// This is NOT a general purpose XML parser.
/// It is not safe to run against untrusted input.
/// It does not work if elements are re-ordered.

int parse_instructions(const char* file_path, const instruction_allocators allocators) {
    const file file = load_file(file_path);
    char* data = file.data;
    char* end = file.data + file.size;

    // We'll build up the partial instruction on the stack to track common items
    // then push each of the variants ot the allocator when we get there.
    instruction instruction;

    // This is the only one actually checked.
    instruction.mnemonic = NULL;

    // Get the detailed name (i.e. MOV (register))
    data = SKIP_UNTIL_AFTER(data, "<heading>", end);
    char* end_of_heading = skip_until(data, "</heading>", end);
    instruction.name = allocate_bytes(allocators.general_allocator, end_of_heading - data + 1, 1);
    strncpy(instruction.name, data, end_of_heading - data);
    instruction.name[end_of_heading - data] = '\0';
    data = end_of_heading + sizeof("</heading>") - 1;

    // Get the description fields
    data = SKIP_UNTIL_AFTER(data, "<desc>", end);
    data = parse_description(data, &instruction, allocators, end);

    // Get the variant info
    data = SKIP_UNTIL_AFTER(data, "<classes>", end);
    int parsedCount = 0;
    data = parse_classes(data, &instruction, allocators, end, &parsedCount);

    munmap(file.data, file.size);

    return parsedCount;
}

static int is_match(const char* data, const char* pattern, const char* end) {
    while (*pattern != '\0' && data < end) {
        if (*data != *pattern) return 0;
        data++;
        pattern++;
    }
    return 1;
}

static char* skip_until(char* data, const char* pattern, const char* end) {
    while (!is_match(data, pattern, end)) {
        data++;
    }

    return data;
}

static char* skip_whitespace(char* data, const char* end) {
    while (data < end) {
        if (*data != ' ' && *data != '\t' && *data != '\n' && *data != '\r') {
            break;
        }
        data++;
    }

    return data;
}

static char* parse_description(char* data, instruction* instruction, instruction_allocators allocators, const char* end) {
    // Get the brief description (i.e. Move register value)
    data = SKIP_UNTIL_AFTER(data, "<brief>", end);
    data = SKIP_UNTIL_AFTER(data, "<para>", end);
    char* end_of_paragraph = skip_until(data, "</para>", end);
    instruction->short_description = allocate_bytes(allocators.general_allocator, end_of_paragraph - data + 2, 1);
    strncpy(instruction->short_description, data, end_of_paragraph - data);
    instruction->short_description[end_of_paragraph - data] = '\n';
    instruction->short_description[end_of_paragraph - data + 1] = '\0';
    data = end_of_paragraph + sizeof("</para>") - 1;

    // Get the long description (i.e. This instruction copies the value in a source register to the destination register.)
    data = SKIP_UNTIL_AFTER(data, "<authored>", end);
    char* end_of_authored = skip_until(data, "</authored>", end);
    instruction->description = allocate_bytes(allocators.general_allocator, 0, 1);
    while (data < end_of_authored) {
        data = SKIP_UNTIL_AFTER(data, "<para>", end_of_authored);
        end_of_paragraph = skip_until(data, "</para>", end_of_authored);
        char* paragraph = allocate_bytes(allocators.general_allocator, end_of_paragraph - data + 1, 1);
        strncpy(paragraph, data, end_of_paragraph - data);
        paragraph[end_of_paragraph - data] = '\n';
        data = end_of_paragraph + sizeof("</para>") - 1;
        data = skip_whitespace(data, end);
    }
    char* paragraph_end = allocate_bytes(allocators.general_allocator, 1, 1);
    paragraph_end[0] = '\0';

    return data + sizeof("</authored>") - 1;
}

static char* parse_classes(char* data, instruction* inst, const instruction_allocators allocators, const char* end, int* parsedCount) {
    char* end_of_classes = skip_until(data, "</classes>", end);
    while (data < end_of_classes) {
        // Get the variant name
        data = SKIP_UNTIL_AFTER(data, "<iclass name=\"", end_of_classes);
        char* end_of_class = skip_until(data, "</iclass>", end_of_classes);
        char* end_of_name = skip_until(data, "\"", end_of_class);
        inst->variant_name = allocate_bytes(allocators.general_allocator, end_of_name - data + 1, 1);
        strncpy(inst->variant_name, data, end_of_name - data);
        inst->variant_name[end_of_name - data] = '\0';
        data = end_of_name + 1;

        // Get the mnemonic (i.e. ORR) - each variant also lists a mnemonic, but I think it's always the same
        char* end_of_docvars = skip_until(data, "</docvars>", end_of_class);
        data = SKIP_UNTIL_AFTER(data, "<docvar key=\"mnemonic\" value=\"", end_of_docvars);
        char* end_of_mnemonic = skip_until(data, "\"", end_of_docvars);
        if (end_of_mnemonic != end_of_docvars) {
            inst->mnemonic = allocate_bytes(allocators.general_allocator, end_of_mnemonic - data + 1, 1);
            strncpy(inst->mnemonic, data, end_of_mnemonic - data);
            inst->mnemonic[end_of_mnemonic - data] = '\0';
            data = end_of_mnemonic + 1;
        }

        data = parse_regdiagram(data, inst, end_of_class);
        if (inst->mnemonic == NULL) {
            char* bits = inst->bits;
            do {
                memcpy(inst->bits, bits, 32);
                data = parse_encoding(data, inst, allocators, end_of_class);

                instruction* allocated_instruction = allocate_bytes(allocators.instruction_allocator, sizeof(instruction), sizeof(char*));
                *allocated_instruction = *inst;
                *parsedCount += 1;

                data = skip_until(data, "<encoding", end_of_class);
            } while (data < end_of_class);

        }
        else {
            instruction* allocated_instruction = allocate_bytes(allocators.instruction_allocator, sizeof(instruction), sizeof(char*));
            *allocated_instruction = *inst;
            *parsedCount += 1;
        }

        data = SKIP_UNTIL_AFTER(data, "</iclass>", end_of_classes);
        data = skip_whitespace(data, end_of_classes);
    }

    return SKIP_UNTIL_AFTER(data, "</classes>", end);
}

static char* parse_regdiagram(char* data, instruction* instruction, const char* end) {
    data = SKIP_UNTIL_AFTER(data, "<regdiagram", end);
    char* end_of_diagram = skip_until(data, "</regdiagram>", end);
    data = SKIP_UNTIL_AFTER(data, "form=\"", end_of_diagram);
    if (data[0] != '3' || data[1] != '2') {
        fprintf(stderr, "expected form to be 32");
        exit(1);
    }

    parse_bitpattern(data, instruction->bits, end_of_diagram);

    return end_of_diagram + sizeof("</regdiagram>") - 1;
}

static char* parse_encoding(char* data, instruction* instruction, instruction_allocators allocators, const char* end) {
    data = SKIP_UNTIL_AFTER(data, "<encoding", end);
    data = SKIP_UNTIL_AFTER(data, "<docvar key=\"mnemonic\" value=\"", end);
    char* end_of_mnemonic = skip_until(data, "\"", end);

    instruction->mnemonic = allocate_bytes(allocators.general_allocator, end_of_mnemonic - data + 1, 1);
    strncpy(instruction->mnemonic, data, end_of_mnemonic - data);
    instruction->mnemonic[end_of_mnemonic - data] = '\0';
    data = end_of_mnemonic + 1;

    char* end_of_bits = skip_until(data, "<asmtemplate>", end);
    parse_bitpattern(data, instruction->bits, end_of_bits);

    return SKIP_UNTIL_AFTER(data, "</encoding>", end);
}

static char* parse_bitpattern(char* data, char* bits, const char* end) {
    while (data < end) {
        // Get hibit
        data = SKIP_UNTIL_AFTER(data, "<box hibit=\"", end);
        char* end_of_hibit = skip_until(data, "\"", end);
        char bit_index_fmt[7];
        snprintf(bit_index_fmt, sizeof(bit_index_fmt), "%%%ldd", end_of_hibit - data);
        char bit_index;
        if (sscanf(data, bit_index_fmt, &bit_index) != 1) {
            fprintf(stderr, "failed reading hibit");
            exit(1);
        }

        data += 3;

        // get bits
        char* end_of_box = skip_until(data, "</box>", end);
        while (data < end_of_box) {
            data = SKIP_UNTIL_AFTER(data, "<c", end_of_box);
            if (data[0] == '>') {
                data++;
                bits[bit_index] = data[0] == '(' ? data[1] : data[0];
                bit_index--;
                data = SKIP_UNTIL_AFTER(data, "</c>", end_of_box);
                data = skip_whitespace(data, end_of_box);
            } else if (data[0] == '/') {
                bit_index--;
                data += 2;
                data = skip_whitespace(data, end_of_box);
            } else {
                data = SKIP_UNTIL_AFTER(data, "colspan=\"", end_of_box);
                char* end_of_colspan = skip_until(data, "\"", end_of_box);
                char colspan_fmt[7]; // %31hhd\0
                snprintf(colspan_fmt, sizeof(colspan_fmt), "%%%dhhd", end_of_colspan - data);
                char column_count;
                if (sscanf(data, colspan_fmt, &column_count) != 1) {
                    fprintf(stderr, "failed reading colspan");
                    exit(1);
                }

                for (int i = 0; i < column_count; i++) {
                    bits[bit_index - i] = 'x';
                }

                bit_index -= column_count;

                data = SKIP_UNTIL_AFTER(data, "/>", end_of_box);
                data = skip_whitespace(data, end_of_box);
            }
        }
        data = end_of_box + sizeof("</box>") - 1;
        data = skip_whitespace(data, end);
    }

    return data;
}
