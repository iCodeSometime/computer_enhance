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


#include "util/preamble.h"
#include "util/virtual_arena/virtual_arena.h"

#include "instruction_set.h"
#include "util/file_loader/file_loader.h"

#define SKIP_UNTIL_AFTER(data, pattern, end) skip_until(data, pattern, end) + sizeof(pattern) - 1

static int is_match(const char* data, const char* pattern, const char* end);
static char* skip_until(char* data, const char* pattern, const char* end);
static char* skip_whitespace(char* data, const char* end);

static char* parse_description(char* data, instruction* instruction, const instruction_allocators allocators, const char* end);
static char* parse_classes(char* data, instruction* instruction, const instruction_allocators allocators, const char* end, int* parsedCount);
static char* parse_bitpattern(char* data, instruction* instruction, instruction_allocators allocators, const char* end);

/// Parse the instruction set XML that ARM publishes.
/// This is NOT a general purpose XML parser.
/// It is not safe to run against untrusted input.
/// It does not work if elements are re-ordered.

int parse_instructions(const char* file_name, const instruction_allocators allocators) {
    const file file = load_file(file_name);
    char* data = file.data;
    char* end = file.data + file.size;

    // We'll build up the partial instruction on the stack to track common items
    // then push each of the variants ot the allocator when we get there.
    instruction instruction;

    // Get the mnemonic (i.e. ORR) - each variant also lists a mnemonic, but I think it's always the same
    data = SKIP_UNTIL_AFTER(data, "<docvar key=\"mnemonic\" value=\"", end);
    char* end_of_mnemonic = skip_until(data, "\"", end);
    instruction.mnemonic = allocate_bytes(allocators.general_allocator, end_of_mnemonic - data + 1, 1);
    strncpy(instruction.mnemonic, data, end_of_mnemonic - data);
    instruction.mnemonic[end_of_mnemonic - data] = '\0';
    data = end_of_mnemonic + 1;

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
    instruction->short_description[end_of_paragraph - data - 1] = '\n';
    instruction->short_description[end_of_paragraph - data] = '\0';
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
        skip_whitespace(data, end);
    }
    char* paragraph_end = allocate_bytes(allocators.general_allocator, 1, 1);
    paragraph_end[0] = '\0';

    return data + sizeof("</authored>") - 1;
}

static char* parse_classes(char* data, instruction* inst, instruction_allocators allocators, const char* end) {
    char* end_of_classes = skip_until(data, "</classes>", end);
    while (data < end_of_classes) {
        // Get the variant name
        data = SKIP_UNTIL_AFTER(data, "<iclass", end_of_classes);
        data = SKIP_UNTIL_AFTER(data, "name=\"", end_of_classes);
        char* end_of_name = skip_until(data, "\"", end);
        inst->variant_name = allocate_bytes(allocators.general_allocator, end_of_name - data + 1, 1);
        strncpy(inst->variant_name, data, end_of_name - data);
        inst->variant_name[end_of_name - data - 1] = '\0';
        data = end_of_name + 1;

        data = parse_bitpattern(data, inst, allocators, end);
        instruction* allocated_instruction = allocate_bytes(allocators.instruction_allocator, sizeof(instruction), sizeof(char*));
        *allocated_instruction = *inst;
    }

    return data;
}

static char* parse_bitpattern(char* data, instruction* instruction, instruction_allocators allocators, const char* end) {
    // bitpattern is always 32 chars, no need for null terminator
    instruction->bits = allocate_bytes(allocators.bitpattern_allocator, 32, 32);

    data = SKIP_UNTIL_AFTER(data, "<regdiagram", end);
    char* end_of_diagram = skip_until(data, "</regdiagram>", end);
    data = SKIP_UNTIL_AFTER(data, "form=\"", end_of_diagram);
    if (data[0] != 3 || data[1] != 2) {
        fprintf(stderr, "expected form to be 32");
        exit(1);
    }

    while (data < end_of_diagram) {
        // Get hibit
        data = SKIP_UNTIL_AFTER(data, "<box hibit=\"", end_of_diagram);
        char bit_index;
        if (sscanf(data, "%2hhd", &bit_index) != 1) {
            fprintf(stderr, "failed reading hibit");
            exit(1);
        }
        data += 3;

        // get bits
        char* end_of_box = skip_until(data, "</box>", end_of_diagram);
        while (data < end_of_box) {
            data = SKIP_UNTIL_AFTER(data, "<c", end_of_box);
            if (data[0] == '>') {
                data++;
                instruction->bits[bit_index] = data[0];
                bit_index++;
            } else {
                data = SKIP_UNTIL_AFTER(data, "colspan=\"", end_of_box);
                char* end_of_colspan = skip_until(data, "\"", end_of_box);
                char fmt[7]; // %31hhd\0
                snprintf(fmt, sizeof(fmt), "%%%dhhd", end_of_colspan - data);
                char column_count;
                if (sscanf(data, fmt, &column_count) != 1) {
                    fprintf(stderr, "failed reading colspan");
                    exit(1);
                }

                for (int i = 0; i < column_count; i++) {
                    instruction->bits[bit_index - i] = 'x';
                }

                bit_index += column_count;
            }
        }
    }

    return end_of_diagram + sizeof("</regdiagram>") - 1;
}
