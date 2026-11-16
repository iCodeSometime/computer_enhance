//
// Created by Kenneth Cochran on 11/2/26.
//

#include <glob.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "instruction_set.h"
#include "util/preamble.h"
#include "util/virtual_arena/virtual_arena.h"

typedef struct instruction_masks {
    u32* significance_masks;
    u32* ones_masks;
    u32* zeros_masks;
    u32 count;
} instruction_masks;

typedef struct decision_tree_node decision_tree_node;
struct decision_tree_node {
    char* name;
    // mnemonic will be set if it's a leaf node
    char* mnemonic;
    decision_tree_node* first_child;
    u32 significance_mask;
    u32 ones_mask;
    decision_tree_node* next_sibling;
};

static u32 parse_all_files(instruction_allocators allocators);
static instruction_masks* create_instruction_masks(u32 instruction_count, instruction* instructions, virtual_arena* allocator);
static decision_tree_node* build_decision_tree(instruction* instructions, instruction_masks* masks, u32 instruction_count);

void generate_decision_tree(void) {
    instruction_allocators allocators;
    allocators.bitpattern_allocator = create_arena(GB(1));
    allocators.general_allocator = create_arena(GB(1));
    allocators.instruction_allocator = create_arena(GB(1));

    instruction* instructions = allocate_bytes(allocators.instruction_allocator, 0, sizeof(char*));
    u32 instruction_count = parse_all_files(allocators);
    instruction_masks* masks = create_instruction_masks(instruction_count, instructions, allocators.instruction_allocator);

    decision_tree_node* root_node = build_decision_tree(instructions, masks, instruction_count);
}

static u32 parse_all_files(instruction_allocators allocators) {
    glob_t files;

    if (glob(XML_PATH("*.xml"), GLOB_ERR | GLOB_NOSORT, NULL, &files) != 0) {
        fprintf(stderr, "Error globbing files - %s\n", XML_PATH("*.xml"));
        exit(1);
    }

    u32 instruction_count = 0;
    for (u64 i = 0; i < files.gl_pathc; i++) {
        instruction_count += parse_instructions(files.gl_pathv[i], allocators);
    }

    return instruction_count;
}

static instruction_masks* create_instruction_masks(u32 instruction_count, instruction* instructions, virtual_arena* allocator) {
    instruction_masks* masks = allocate_bytes(allocator, sizeof(instruction_masks), sizeof(instruction_masks));
    masks->count = instruction_count;
    masks->significance_masks = allocate_bytes(allocator, masks->count * sizeof(u32), sizeof(u32));
    masks->ones_masks = allocate_bytes(allocator, masks->count * sizeof(u32), sizeof(u32));
    masks->zeros_masks = allocate_bytes(allocator, masks->count * sizeof(u32), sizeof(u32));

    for (u32 i = 0; i < masks->count; i++) {
        u32 significance = 0;
        u32 ones = 0;
        u32 zeros = 0;

        for (u32 bit = 0; bit < 32; bit++) {
            char cell = instructions[i].bits[bit];
            if (cell == 'x') {
                continue;
            }

            significance |= 1u << bit;
            if (cell == '1') {
                ones |= 1u << bit;
            } else {
                zeros |= 1u << bit;
            }
        }

        masks->significance_masks[i] = significance;
        masks->ones_masks[i] = ones;
        masks->zeros_masks[i] = zeros;
    }

    return masks;
}

static void get_masks(u32* array, u32 shift) {
    u32 mask_updates[16] = {
        0b1111 << shift,
        0b1110 << shift,
        0b1101 << shift,
        0b1100 << shift,
        0b1011 << shift,
        0b1010 << shift,
        0b1001 << shift,
        0b1000 << shift,
        0b0111 << shift,
        0b0110 << shift,
        0b0101 << shift,
        0b0100 << shift,
        0b0011 << shift,
        0b0010 << shift,
        0b0001 << shift,
        0b0000 << shift,
    };

    memcpy(array, mask_updates, sizeof(mask_updates));
}

static char* get_name_from_mask(u32 significance_mask, u32 ones_mask, char data[33]) {
    for (u32 i = 0; i < 32; i++) {
        if (((significance_mask >> i) & 1) == 0) {
            data[i] = 'x';
        } else {
            if (((ones_mask >> i) & 1) == 0) {
                data[i] = '0';
            } else {
                data[i] = '1';
            }
        }
    }

    data[32] = '\0';
    return data;
}

// relevant_mask is all combinations of four contiguous bits
static decision_tree_node* build_decision_tree_internal(instruction* instructions, instruction_masks* masks, u32 relevant_indices[], u32 relevant_indices_count, u32 relevant_mask, u32 ones_mask, u32 shift, virtual_arena* relevant_indices_allocator, virtual_arena* node_allocator, virtual_arena* general_allocator) {
    u32 significance_masks[16];
    u32 ones_masks[16];
    get_masks(significance_masks, shift);
    get_masks(ones_masks, shift);
    decision_tree_node* parent_node = allocate_bytes(node_allocator, sizeof(decision_tree_node), sizeof(u32*));
    parent_node->name = allocate_bytes(general_allocator, 33, 1);
    get_name_from_mask(relevant_mask, ones_mask, parent_node->name);
    parent_node->ones_mask = ones_mask;
    parent_node->significance_mask = relevant_mask;
    parent_node->mnemonic = NULL;
    decision_tree_node* previous_sibling = NULL;

    for (u32 i = 0; i < 16; i++) {
        u32 current_significance_mask = significance_masks[i];

        u32* matching_significance_items = allocate_bytes(relevant_indices_allocator, 0, sizeof(u32*));
        u32 matching_significance_count = 0;
        for (u32 cur = 0; cur < relevant_indices_count; cur++) {
            u32 current_index = relevant_indices[cur];
            if ((masks->significance_masks[current_index] & (0xF << shift)) == current_significance_mask) {
                *((u32*)allocate_bytes(relevant_indices_allocator, sizeof(u32), sizeof(u32))) = current_index;
                matching_significance_count++;
            }
        }

        for (u32 j = 0; j < 16; j++) {
            u32 current_ones_mask = ones_masks[j];

            u32* new_relevant_items = allocate_bytes(relevant_indices_allocator, 0, sizeof(u32*));
            u32 new_relevant_items_count = 0;
            for (u32 cur = 0; cur < matching_significance_count; cur++) {
                u32 current_index = matching_significance_items[cur];

                if ((masks->ones_masks[current_index] & current_significance_mask) == current_ones_mask) {
                    *((u32*)allocate_bytes(relevant_indices_allocator, sizeof(u32*), sizeof(u32))) = current_index;
                    new_relevant_items_count++;
                }
            }

            if (new_relevant_items_count == 0) continue;
            else if (new_relevant_items_count == 1) {
                decision_tree_node* node = allocate_bytes(node_allocator, sizeof(decision_tree_node), sizeof(u32*));
                node->first_child = NULL;
                node->mnemonic = instructions[*new_relevant_items].mnemonic;
                node->name = node->mnemonic;
                node->significance_mask = current_significance_mask;
                node->ones_mask = current_ones_mask;
                node->next_sibling = previous_sibling;
                previous_sibling = node;
            } else {
                decision_tree_node* node = build_decision_tree_internal(instructions, masks, new_relevant_items, new_relevant_items_count, current_significance_mask, current_ones_mask, shift + 4, relevant_indices_allocator, node_allocator, general_allocator);

                node->next_sibling = previous_sibling;
                previous_sibling = node;
            }
        }
        parent_node->first_child = previous_sibling;
    }

    return parent_node;
}

static decision_tree_node* build_decision_tree(instruction* instructions, instruction_masks* masks, u32 instruction_count) {
    virtual_arena* relevant_indices_allocator = create_arena(GB(1));
    virtual_arena* node_allocator = create_arena(GB(1));
    virtual_arena* general_allocator = create_arena(GB(1));

    u32 initial_relevant_indices[instruction_count];
    for (u32 i = 0; i < instruction_count; i++) {
        initial_relevant_indices[i] = i;
    }

    return build_decision_tree_internal(instructions, masks, initial_relevant_indices, instruction_count, 0, 0, 0, relevant_indices_allocator, node_allocator, general_allocator);
}

