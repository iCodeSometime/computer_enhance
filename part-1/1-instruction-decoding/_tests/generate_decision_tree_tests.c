//
// Tests for generate_decision_tree.c — intended behavior.
// Includes the implementation so static helpers can be exercised without
// changing the source under test.
//

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "util/_tests/tests.h"
#include "util/preamble.h"
#include "util/virtual_arena/virtual_arena.h"

#include "../instruction_set/instruction_set.h"

#include "../instruction_set/generate_decision_tree.c"

static void fill_bits(instruction* inst, const char* msb_first) {
    // msb_first[0] is instruction bit 31 → stored in bits[31]
    for (u32 i = 0; i < 32; i++) {
        inst->bits[i] = msb_first[31 - i];
    }
}

static instruction make_instruction(const char* mnemonic, const char* msb_first) {
    instruction inst = {0};
    inst.mnemonic = (char*)mnemonic;
    inst.name = (char*)mnemonic;
    fill_bits(&inst, msb_first);
    return inst;
}

static u32 count_leaves(const decision_tree_node* node) {
    if (node == NULL) {
        return 0;
    }
    if (node->mnemonic != NULL) {
        return 1 + count_leaves(node->next_sibling);
    }
    return count_leaves(node->first_child) + count_leaves(node->next_sibling);
}

static u32 count_leaves_with_mnemonic(const decision_tree_node* node, const char* mnemonic) {
    if (node == NULL) {
        return 0;
    }
    u32 count = 0;
    if (node->mnemonic != NULL && strcmp(node->mnemonic, mnemonic) == 0) {
        count = 1;
    }
    return count + count_leaves_with_mnemonic(node->first_child, mnemonic)
                 + count_leaves_with_mnemonic(node->next_sibling, mnemonic);
}

static u32 max_depth(const decision_tree_node* node) {
    if (node == NULL) {
        return 0;
    }
    u32 child_depth = max_depth(node->first_child);
    u32 sibling_depth = max_depth(node->next_sibling);
    u32 depth_here = node->mnemonic != NULL ? 1 : 1 + child_depth;
    return depth_here > sibling_depth ? depth_here : sibling_depth;
}

static bool has_child_with_significance(const decision_tree_node* parent, u32 significance_mask) {
    for (const decision_tree_node* child = parent->first_child; child != NULL; child = child->next_sibling) {
        if (child->significance_mask == significance_mask) {
            return true;
        }
    }
    return false;
}

static void test_create_instruction_masks_empty_and_fixed(void) {
    virtual_arena* allocator = create_arena(MB(1));

    instruction instructions[3];
    instructions[0] = make_instruction("ALLX", "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    instructions[1] = make_instruction("ALL1", "11111111111111111111111111111111");
    instructions[2] = make_instruction("ALL0", "00000000000000000000000000000000");

    instruction_masks* masks = create_instruction_masks(3, instructions, allocator);

    EXPECT(masks->count == 3);

    EXPECT(masks->significance_masks[0] == 0);
    EXPECT(masks->ones_masks[0] == 0);
    EXPECT(masks->zeros_masks[0] == 0);

    EXPECT(masks->significance_masks[1] == 0xFFFFFFFFu);
    EXPECT(masks->ones_masks[1] == 0xFFFFFFFFu);
    EXPECT(masks->zeros_masks[1] == 0);

    EXPECT(masks->significance_masks[2] == 0xFFFFFFFFu);
    EXPECT(masks->ones_masks[2] == 0);
    EXPECT(masks->zeros_masks[2] == 0xFFFFFFFFu);

    destroy_arena(allocator);
}

static void test_create_instruction_masks_mixed_pattern(void) {
    virtual_arena* allocator = create_arena(MB(1));

    // MSB-first: bit31='1', bit30='0', bit29='x', ... low nibble bit3..0 = 1,0,x,1
    // bits[] is LSB-first: bits[0]=bit0 of instruction.
    instruction instructions[1];
    instructions[0] = make_instruction("MIX", "10xxxxxxxxxxxxxxxxxxxxxxxxxx10x1");

    instruction_masks* masks = create_instruction_masks(1, instructions, allocator);

    // bit0='1', bit1='x', bit2='0', bit3='1', bit30='0', bit31='1'
    u32 expected_significance = (1u << 0) | (1u << 2) | (1u << 3) | (1u << 30) | (1u << 31);
    u32 expected_ones = (1u << 0) | (1u << 3) | (1u << 31);
    u32 expected_zeros = (1u << 2) | (1u << 30);

    EXPECT(masks->significance_masks[0] == expected_significance);
    EXPECT(masks->ones_masks[0] == expected_ones);
    EXPECT(masks->zeros_masks[0] == expected_zeros);

    destroy_arena(allocator);
}

static void test_get_masks_shift_zero(void) {
    u32 masks[16];
    // Poison so a short memcpy is visible.
    memset(masks, 0xA5, sizeof(masks));

    get_masks(masks, 0);

    const u32 expected[16] = {
        0b1111, 0b1110, 0b1101, 0b1100,
        0b1011, 0b1010, 0b1001, 0b1000,
        0b0111, 0b0110, 0b0101, 0b0100,
        0b0011, 0b0010, 0b0001, 0b0000,
    };

    for (u32 i = 0; i < 16; i++) {
        EXPECT_WITH_MSG(masks[i] == expected[i], "get_masks(shift=0) must write all 16 entries");
    }
}

static void test_get_masks_shift_four(void) {
    u32 masks[16];
    memset(masks, 0xA5, sizeof(masks));

    get_masks(masks, 4);

    for (u32 i = 0; i < 16; i++) {
        u32 nibble = 15u - i;
        EXPECT(masks[i] == (nibble << 4));
    }
}

static void test_get_masks_shift_twenty_eight(void) {
    u32 masks[16];
    memset(masks, 0xA5, sizeof(masks));

    get_masks(masks, 28);

    for (u32 i = 0; i < 16; i++) {
        u32 nibble = 15u - i;
        EXPECT(masks[i] == (nibble << 28));
    }
}

static void test_get_name_from_mask(void) {
    char data[33];
    memset(data, '?', sizeof(data));

    // significance bit0+bit1, ones only bit0 → "...xx01" in LSB-first string order
    get_name_from_mask(0b11u, 0b01u, data);

    EXPECT(data[32] == '\0');
    EXPECT(data[0] == '1');
    EXPECT(data[1] == '0');
    for (u32 i = 2; i < 32; i++) {
        EXPECT(data[i] == 'x');
    }

    get_name_from_mask(0, 0, data);
    for (u32 i = 0; i < 32; i++) {
        EXPECT(data[i] == 'x');
    }
    EXPECT(data[32] == '\0');

    get_name_from_mask(0xFFFFFFFFu, 0xFFFFFFFFu, data);
    for (u32 i = 0; i < 32; i++) {
        EXPECT(data[i] == '1');
    }

    get_name_from_mask(0xFFFFFFFFu, 0, data);
    for (u32 i = 0; i < 32; i++) {
        EXPECT(data[i] == '0');
    }
}

static void test_decision_tree_single_instruction(void) {
    virtual_arena* allocator = create_arena(MB(1));

    instruction instructions[1];
    instructions[0] = make_instruction("NOP", "11010101000000110010000000011111");

    instruction_masks* masks = create_instruction_masks(1, instructions, allocator);
    decision_tree_node* root = build_decision_tree(instructions, masks, 1);

    EXPECT(root != NULL);
    EXPECT(count_leaves_with_mnemonic(root, "NOP") >= 1);
    EXPECT(max_depth(root) <= 9);

    destroy_arena(allocator);
}

static void test_decision_tree_distinguishes_low_nibble(void) {
    virtual_arena* allocator = create_arena(MB(1));

    // Differ only in bit 0; all other bits fixed identical.
    instruction instructions[2];
    instructions[0] = make_instruction("A", "00000000000000000000000000000001");
    instructions[1] = make_instruction("B", "00000000000000000000000000000000");

    instruction_masks* masks = create_instruction_masks(2, instructions, allocator);
    decision_tree_node* root = build_decision_tree(instructions, masks, 2);

    EXPECT(root != NULL);
    EXPECT(root->first_child != NULL);

    // Must terminate within one node per nibble (+ root).
    EXPECT(max_depth(root) <= 9);

    EXPECT(count_leaves_with_mnemonic(root, "A") >= 1);
    EXPECT(count_leaves_with_mnemonic(root, "B") >= 1);

    // Discriminating children for the low nibble must be retained, not only the
    // all-don't-care (significance 0) grouping.
    EXPECT(has_child_with_significance(root, 0b1111u)
           || has_child_with_significance(root, 0b0001u));

    destroy_arena(allocator);
}

static void test_decision_tree_distinguishes_high_nibble(void) {
    virtual_arena* allocator = create_arena(MB(1));

    // Same low 28 bits; differ in bits 31..28 so the split happens at a later shift.
    instruction instructions[2];
    instructions[0] = make_instruction("HI", "00010000000000000000000000000000");
    instructions[1] = make_instruction("LO", "00000000000000000000000000000000");

    instruction_masks* masks = create_instruction_masks(2, instructions, allocator);
    decision_tree_node* root = build_decision_tree(instructions, masks, 2);

    EXPECT(root != NULL);
    EXPECT(max_depth(root) <= 9);
    EXPECT(count_leaves_with_mnemonic(root, "HI") >= 1);
    EXPECT(count_leaves_with_mnemonic(root, "LO") >= 1);

    destroy_arena(allocator);
}

static void test_decision_tree_groups_dont_care_compatible(void) {
    virtual_arena* allocator = create_arena(MB(1));

    // Two patterns that share a constrained low bit and wildcards elsewhere.
    instruction instructions[2];
    instructions[0] = make_instruction("X1", "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx1");
    instructions[1] = make_instruction("Y1", "xxxxxxxxxxxxxx0xxxxxxxxxxxxxxxx1");

    instruction_masks* masks = create_instruction_masks(2, instructions, allocator);
    decision_tree_node* root = build_decision_tree(instructions, masks, 2);

    EXPECT(root != NULL);
    EXPECT(max_depth(root) <= 9);
    EXPECT(count_leaves_with_mnemonic(root, "X1") >= 1);
    EXPECT(count_leaves_with_mnemonic(root, "Y1") >= 1);
    EXPECT(count_leaves(root) >= 2);

    destroy_arena(allocator);
}

static void test_decision_tree_three_way_split(void) {
    virtual_arena* allocator = create_arena(MB(1));

    instruction instructions[3];
    instructions[0] = make_instruction("S0", "11110000111100001111000011110000");
    instructions[1] = make_instruction("S1", "11110000111100001111000011110001");
    instructions[2] = make_instruction("S2", "11110000111100001111000011110010");

    instruction_masks* masks = create_instruction_masks(3, instructions, allocator);
    decision_tree_node* root = build_decision_tree(instructions, masks, 3);

    EXPECT(root != NULL);
    EXPECT(max_depth(root) <= 9);
    EXPECT(count_leaves_with_mnemonic(root, "S0") >= 1);
    EXPECT(count_leaves_with_mnemonic(root, "S1") >= 1);
    EXPECT(count_leaves_with_mnemonic(root, "S2") >= 1);

    destroy_arena(allocator);
}

static void test_root_node_name_is_unconstrained(void) {
    virtual_arena* allocator = create_arena(MB(1));

    instruction instructions[1];
    instructions[0] = make_instruction("Z", "00000000000000000000000000000000");

    instruction_masks* masks = create_instruction_masks(1, instructions, allocator);
    decision_tree_node* root = build_decision_tree(instructions, masks, 1);

    EXPECT(root != NULL);
    EXPECT(root->significance_mask == 0);
    EXPECT(root->ones_mask == 0);
    EXPECT(root->mnemonic == NULL);
    EXPECT(root->name != NULL);
    EXPECT(strcmp(root->name, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx") == 0);

    destroy_arena(allocator);
}

int run_generate_decision_tree_tests(void) {
    prepare_tests("generate_decision_tree_tests");

    run_test(test_create_instruction_masks_empty_and_fixed, "create_instruction_masks empty/fixed");
    run_test(test_create_instruction_masks_mixed_pattern, "create_instruction_masks mixed pattern");
    run_test(test_get_masks_shift_zero, "get_masks shift 0");
    run_test(test_get_masks_shift_four, "get_masks shift 4");
    run_test(test_get_masks_shift_twenty_eight, "get_masks shift 28");
    run_test(test_get_name_from_mask, "get_name_from_mask");

    run_test(test_root_node_name_is_unconstrained, "root node unconstrained name");
    run_test(test_decision_tree_single_instruction, "decision tree single instruction");
    run_test(test_decision_tree_distinguishes_low_nibble, "decision tree low-nibble split");
    run_test(test_decision_tree_distinguishes_high_nibble, "decision tree high-nibble split");
    run_test(test_decision_tree_groups_dont_care_compatible, "decision tree dont-care compatible");
    run_test(test_decision_tree_three_way_split, "decision tree three-way split");

    printf("%llu passed, %llu failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
