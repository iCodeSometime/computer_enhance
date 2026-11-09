//
// Created by Kenneth Cochran on 10/29/26.
//

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <glob.h>

#include "util/preamble.h"
#include "util/virtual_arena/virtual_arena.h"

#include "../instruction_set/instruction_set.h"
#include "util/_tests/tests.h"

static void run_instruction_test(void (*fn)(instruction_allocators*), instruction_allocators* allocators, const char* name) {
    printf("-- %s\n", name);
    g_in_test = 1;
    const int fault = sigsetjmp(g_fault_jmp, 1);
    if (fault == 0) {
        fn(allocators);
    } else {
        g_failed++;
        fprintf(stderr, "FAIL %s: signal %d\n", name, fault);
    }
    g_in_test = 0;
}

static void test_single_file(const char* file_path, const instruction_allocators* allocators, const u8 expected_instruction_count, const instruction* expected_instructions) {
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
            EXPECT(instructions[i].bits[j] == '1' || instructions[i].bits[j] == '0' || instructions[i].bits[j] == 'x');
        }
    }
}

static void test_add_immediate(instruction_allocators* allocators) {
    instruction instruction = {
        .name = "ADD (immediate)",
        .mnemonic = "ADD",
        .short_description = "Add immediate value\n",
        .description = "This instruction adds a register value and an optionally-shifted\nimmediate value, and writes the result to the destination register.\n",
        .variant_name = "Not setting the condition flags",
        .bits = INSTRUCTION_LITERAL(x00100010xxxxxxxxxxxxxxxxxxxxxxx),
    };

    test_single_file(XML_PATH("add_addsub_imm.xml"), allocators, 1, &instruction);
}

static void test_movz(instruction_allocators* allocators) {
    instruction instruction = {
        .name = "MOVZ",
        .mnemonic = "MOVZ",
        .short_description = "Move wide with zero\n",
        .description = "This instruction moves an optionally-shifted 16-bit immediate\nvalue to a register.\n",
        .variant_name = "Immediate packed into 16-bit value and 2-bit shift",
        .bits = INSTRUCTION_LITERAL(x10100101xxxxxxxxxxxxxxxxxxxxxxx),
    };

    test_single_file(XML_PATH("movz.xml"), allocators, 1, &instruction);
}

static void test_movk(instruction_allocators* allocators) {
    instruction instruction = {
        .name = "MOVK",
        .mnemonic = "MOVK",
        .short_description = "Move wide with keep\n",
        .description = "This instruction moves an optionally-shifted 16-bit immediate value into a\nregister, keeping other bits unchanged.\n",
        .variant_name = "Immediate packed into 16-bit value and 2-bit shift",
        .bits = INSTRUCTION_LITERAL(x11100101xxxxxxxxxxxxxxxxxxxxxxx),
    };

    test_single_file(XML_PATH("movk.xml"), allocators, 1, &instruction);
}

static void test_b_uncond(instruction_allocators* allocators) {
    instruction instruction = {
        .name = "B",
        .mnemonic = "B",
        .short_description = "Branch\n",
        .description = "This instruction branches unconditionally to a label at a PC-relative offset. This instruction provides a hint that this is not a subroutine call or return.\n",
        .variant_name = "26-bit signed PC-relative branch offset",
        .bits = INSTRUCTION_LITERAL(000101xxxxxxxxxxxxxxxxxxxxxxxxxx),
    };

    test_single_file(XML_PATH("b_uncond.xml"), allocators, 1, &instruction);
}

static void test_ret(instruction_allocators* allocators) {
    instruction instruction = {
        .name = "RET",
        .mnemonic = "RET",
        .short_description = "Return from subroutine\n",
        .description = "This instruction branches unconditionally to an address in a register.\n"
                       "This instruction provides a hint that this is a subroutine return.\n",
        .variant_name = "Integer",
        .bits = INSTRUCTION_LITERAL(1101011001011111000000xxxxx00000),
    };

    test_single_file(XML_PATH("ret.xml"), allocators, 1, &instruction);
}

static void test_adr(instruction_allocators* allocators) {
    instruction instruction = {
        .name = "ADR",
        .mnemonic = "ADR",
        .short_description = "Form PC-relative address\n",
        .description = "This instruction adds an immediate value to the PC value to form a\n"
                       "PC-relative address, and writes the result to the destination register.\n",
        .variant_name = "Literal",
        .bits = INSTRUCTION_LITERAL(0xx10000xxxxxxxxxxxxxxxxxxxxxxxx),
    };

    test_single_file(XML_PATH("adr.xml"), allocators, 1, &instruction);
}

static void test_b_cond(instruction_allocators* allocators) {
    instruction instruction = {
        .name = "B.cond",
        .mnemonic = "B",
        .short_description = "Branch conditionally\n",
        .description = "This instruction branches conditionally to a label at a PC-relative\n"
                       "offset.\n"
                       "This instruction provides a hint that this is not a subroutine call or return.\n",
        .variant_name = "19-bit signed PC-relative branch offset",
        .bits = INSTRUCTION_LITERAL(01010100xxxxxxxxxxxxxxxxxxx0xxxx),
    };

    test_single_file(XML_PATH("b_cond.xml"), allocators, 1, &instruction);
}

static void test_tbz(instruction_allocators* allocators) {
    instruction instruction = {
        .name = "TBZ",
        .mnemonic = "TBZ",
        .short_description = "Test bit and branch if zero\n",
        .description = "This instruction compares the value of a test bit with zero,\n"
                       "and conditionally branches to a label at a PC-relative offset if the\n"
                       "comparison is equal.\n"
                       "This instruction provides a hint that this is not a subroutine call or return.\n"
                       "This instruction does not affect condition flags.\n",
        .variant_name = "14-bit signed PC-relative branch offset",
        .bits = INSTRUCTION_LITERAL(x0110110xxxxxxxxxxxxxxxxxxxxxxxx),
    };

    test_single_file(XML_PATH("tbz.xml"), allocators, 1, &instruction);
}

static void test_add_shifted_register(instruction_allocators* allocators) {
    instruction instruction = {
        .name = "ADD (shifted register)",
        .mnemonic = "ADD",
        .short_description = "Add optionally-shifted register\n",
        .description = "This instruction adds a register value and an optionally-shifted\n"
                       "register value, and writes the result to the destination register.\n",
        .variant_name = "Not setting the condition flags",
        .bits = INSTRUCTION_LITERAL(x0001011xx0xxxxxxxxxxxxxxxxxxxxx),
    };

    test_single_file(XML_PATH("add_addsub_shift.xml"), allocators, 1, &instruction);
}

static void test_add_extended_register(instruction_allocators* allocators) {
    instruction instruction = {
        .name = "ADD (extended register)",
        .mnemonic = "ADD",
        .short_description = "Add extended and scaled register\n",
        .description = "This instruction adds a register value and a sign-extended or zero-extended\n"
                       "register value, followed by an optional left shift amount,\n"
                       "and writes the result to the destination register. The argument\n"
                       "that is extended from the <syntax>&lt;Rm&gt;</syntax> register can be a byte, halfword,\n"
                       "word, or doubleword.\n",
        .variant_name = "Not setting the condition flags",
        .bits = INSTRUCTION_LITERAL(x0001011001xxxxxxxxxxxxxxxxxxxxx),
    };

    test_single_file(XML_PATH("add_addsub_ext.xml"), allocators, 1, &instruction);
}

static void test_nop(instruction_allocators* allocators) {
    instruction instruction = {
        .name = "NOP",
        .mnemonic = "NOP",
        .short_description = "No operation\n",
        .description = "This instruction does nothing, other than advance the value of the\n"
                       "program counter by 4. This instruction can be used for instruction\n"
                       "alignment purposes.\n"
                       "The timing effects of including a <instruction>NOP</instruction> instruction in a program are not guaranteed.\n"
                       "It can increase execution time, leave it unchanged, or even reduce it. Therefore, <instruction>NOP</instruction>\n"
                       "instructions are not suitable for timing loops.\n"
                       "\n",
        .variant_name = "System",
        .bits = INSTRUCTION_LITERAL(11010101000000110010000000011111),
    };

    test_single_file(XML_PATH("nop.xml"), allocators, 1, &instruction);
}

static void test_dsb(instruction_allocators* allocators) {
    char* description = "This instruction is a memory barrier that ensures the completion\n"
                        "of memory accesses, see <xref linkend=\"BEICEFJH\">Data Synchronization Barrier</xref>.\n";

    instruction instructions[] = {
        {
            .name = "DSB",
            .mnemonic = "DSB",
            .short_description = "Data synchronization barrier\n",
            .description = description,
            .variant_name = "Memory barrier",
            .bits = INSTRUCTION_LITERAL(11010101000000110011xxxx10011111),
        },
        {
            .name = "DSB",
            .mnemonic = "DSB",
            .short_description = "Data synchronization barrier\n",
            .description = description,
            .variant_name = "Memory nXS barrier",
            .bits = INSTRUCTION_LITERAL(11010101000000110011xx1000111111),
        },
    };

    test_single_file(XML_PATH("dsb.xml"), allocators, 2, instructions);
}

static void test_ldr_immediate(instruction_allocators* allocators) {
    char* description = "This instruction loads a word or doubleword from memory\n"
                              "and writes it to a register.\n"
                              "The address that is used for the load is calculated from a base register\n"
                              "and an immediate offset.\n"
                              "For information about addressing modes, see\n"
                              "<xref linkend=\"CHDIIIBB\">Load/Store addressing modes</xref>.\n"
                              "The Unsigned offset variant scales the immediate offset value by\n"
                              "the size of the value accessed before adding it to the base register value.\n";

    instruction instructions[] = {
        {
            .name = "LDR (immediate)",
            .mnemonic = "LDR",
            .short_description = "Load register (immediate)\n",
            .description = description,
            .variant_name = "Post-index",
            .bits = INSTRUCTION_LITERAL(1x111000010xxxxxxxxx01xxxxxxxxxx),
        },
        {
            .name = "LDR (immediate)",
            .mnemonic = "LDR",
            .short_description = "Load register (immediate)\n",
            .description = description,
            .variant_name = "Pre-index",
            .bits = INSTRUCTION_LITERAL(1x111000010xxxxxxxxx11xxxxxxxxxx),
        },
        {
            .name = "LDR (immediate)",
            .mnemonic = "LDR",
            .short_description = "Load register (immediate)\n",
            .description = description,
            .variant_name = "Unsigned offset",
            .bits = INSTRUCTION_LITERAL(1x11100101xxxxxxxxxxxxxxxxxxxxxx),
        },
    };

    test_single_file(XML_PATH("ldr_imm_gen.xml"), allocators, 3, instructions);
}

static void test_ldapr(instruction_allocators* allocators) {
    char* description = "This instruction derives an address from a base register\n"
                              "value, loads a 32-bit word or 64-bit doubleword from the derived\n"
                              "address in memory, and writes it to a register.\n"
                              "<instruction>LDAPR</instruction> loads from memory with AcquirePC semantics.\n"
                              "For more information about memory ordering semantics, see <xref linkend=\"BEIHCHEF\">Load-Acquire, Load-AcquirePC, and Store-Release</xref>.\n"
                              "For information about addressing modes, see\n"
                              "<xref linkend=\"CHDIIIBB\">Load/Store addressing modes</xref>.\n";

    instruction instructions[] = {
        {
            .name = "LDAPR",
            .mnemonic = "LDAPR",
            .short_description = "Load-acquire RCpc register\n",
            .description = description,
            .variant_name = "Post-index",
            .bits = INSTRUCTION_LITERAL(1x01100111000000000010xxxxxxxxxx),
        },
        {
            .name = "LDAPR",
            .mnemonic = "LDAPR",
            .short_description = "Load-acquire RCpc register\n",
            .description = description,
            .variant_name = "No offset",
            .bits = INSTRUCTION_LITERAL(1x11100010111111110000xxxxxxxxxx)
        },
    };

    test_single_file(XML_PATH("ldapr.xml"), allocators, 2, instructions);
}

static void test_mov_register(instruction_allocators* allocators) {
    instruction instruction = {
        .name = "MOV (register)",
        .mnemonic = "ORR",
        .short_description = "Move register value\n",
        .description = "This instruction copies the value in a source register to the destination register.\n",
        .variant_name = "Not setting the condition flags",
        .bits = INSTRUCTION_LITERAL(x0101010000xxxxx00000011111xxxxx),
    };

    test_single_file(XML_PATH("mov_orr_log_shift.xml"), allocators, 1, &instruction);
}

static void test_bl(instruction_allocators* allocators) {
    instruction instruction = {
        .name = "BL",
        .mnemonic = "BL",
        .short_description = "Branch with link\n",
        .description = "This instruction branches to a PC-relative offset, setting\n"
                       "register X30 to PC+4. This instruction provides a hint that this is a subroutine call.\n",
        .variant_name = "26-bit signed PC-relative branch offset",
        .bits = INSTRUCTION_LITERAL(100101xxxxxxxxxxxxxxxxxxxxxxxxxx),
    };

    test_single_file(XML_PATH("bl.xml"), allocators, 1, &instruction);
}

static void test_mov_bitmask_immediate(instruction_allocators* allocators) {
    instruction instruction = {
        .name = "MOV (bitmask immediate)",
        .mnemonic = "ORR",
        .short_description = "Move bitmask immediate value\n",
        .description = "This instruction writes a bitmask immediate\n"
                       "value to a register.\n",
        .variant_name = "Not setting the condition flags",
        .bits = INSTRUCTION_LITERAL(x01100100xxxxxxxxxxxxx11111xxxxx),
    };

    test_single_file(XML_PATH("mov_orr_log_imm.xml"), allocators, 1, &instruction);
}

static void test_str_immediate(instruction_allocators* allocators) {
    char* description = "This instruction stores a word or a doubleword from a\n"
                        "register to memory. The address that is used for the store is\n"
                        "calculated from a base register and an immediate offset.\n"
                        "For information about addressing modes, see\n"
                        "<xref linkend=\"CHDIIIBB\">Load/Store addressing modes</xref>.\n";

    instruction instructions[] = {
        {
            .name = "STR (immediate)",
            .mnemonic = "STR",
            .short_description = "Store register (immediate)\n",
            .description = description,
            .variant_name = "Post-index",
            .bits = INSTRUCTION_LITERAL(1x111000000xxxxxxxxx01xxxxxxxxxx),
        },
        {
            .name = "STR (immediate)",
            .mnemonic = "STR",
            .short_description = "Store register (immediate)\n",
            .description = description,
            .variant_name = "Pre-index",
            .bits = INSTRUCTION_LITERAL(1x111000000xxxxxxxxx11xxxxxxxxxx),
        },
        {
            .name = "STR (immediate)",
            .mnemonic = "STR",
            .short_description = "Store register (immediate)\n",
            .description = description,
            .variant_name = "Unsigned offset",
            .bits = INSTRUCTION_LITERAL(1x11100100xxxxxxxxxxxxxxxxxxxxxx),
        },
    };

    test_single_file(XML_PATH("str_imm_gen.xml"), allocators, 3, instructions);
}

static void test_cbz(instruction_allocators* allocators) {
    instruction instruction = {
        .name = "CBZ",
        .mnemonic = "CBZ",
        .short_description = "Compare and branch on zero\n",
        .description = "This instruction compares the value in a register with zero,\n"
                       "and conditionally branches to a label at a PC-relative offset if the\n"
                       "comparison is equal.\n"
                       "This instruction provides a hint that this is not a subroutine call or return.\n"
                       "This instruction does not affect condition flags.\n",
        .variant_name = "19-bit signed PC-relative branch offset",
        .bits = INSTRUCTION_LITERAL(x0110100xxxxxxxxxxxxxxxxxxxxxxxx),
    };

    test_single_file(XML_PATH("cbz.xml"), allocators, 1, &instruction);
}

static void test_cmp_immediate(instruction_allocators* allocators) {
    instruction instruction = {
        .name = "CMP (immediate)",
        .mnemonic = "SUBS",
        .short_description = "Compare (immediate)\n",
        .description = "This instruction subtracts an optionally-shifted immediate\n"
                       "value from a register value. It updates the condition flags based\n"
                       "on the result, and discards the result.\n",
        .variant_name = "Setting the condition flags",
        .bits = INSTRUCTION_LITERAL(x11100010xxxxxxxxxxxxxxxxxx11111),
    };

    test_single_file(XML_PATH("cmp_subs_addsub_imm.xml"), allocators, 1, &instruction);
}

static void test_ldp(instruction_allocators* allocators) {
    char* description = "This instruction calculates an\n"
                        "address from a base register value and an immediate offset,\n"
                        "loads two 32-bit words or two\n"
                        "64-bit doublewords from memory,\n"
                        "and writes them to\n"
                        "two registers.\n"
                        "For information about addressing modes, see\n"
                        "<xref linkend=\"CHDIIIBB\">Load/Store addressing modes</xref>.\n";

    instruction instructions[] = {
        {
            .name = "LDP",
            .mnemonic = "LDP",
            .short_description = "Load pair of registers\n",
            .description = description,
            .variant_name = "Post-index",
            .bits = INSTRUCTION_LITERAL(x010100011xxxxxxxxxxxxxxxxxxxxxx),
        },
        {
            .name = "LDP",
            .mnemonic = "LDP",
            .short_description = "Load pair of registers\n",
            .description = description,
            .variant_name = "Pre-index",
            .bits = INSTRUCTION_LITERAL(x010100111xxxxxxxxxxxxxxxxxxxxxx),
        },
        {
            .name = "LDP",
            .mnemonic = "LDP",
            .short_description = "Load pair of registers\n",
            .description = description,
            .variant_name = "Signed offset",
            .bits = INSTRUCTION_LITERAL(x010100101xxxxxxxxxxxxxxxxxxxxxx),
        },
    };

    test_single_file(XML_PATH("ldp_gen.xml"), allocators, 3, instructions);
}

static void test_stp(instruction_allocators* allocators) {
    char* description = "This instruction calculates an\n"
                        "address from a base register value and an immediate offset,\n"
                        "and stores two 32-bit words or\n"
                        "two 64-bit doublewords to the calculated address,\n"
                        "from two registers.\n"
                        "For information about addressing modes, see\n"
                        "<xref linkend=\"CHDIIIBB\">Load/Store addressing modes</xref>.\n";

    instruction instructions[] = {
        {
            .name = "STP",
            .mnemonic = "STP",
            .short_description = "Store pair of registers\n",
            .description = description,
            .variant_name = "Post-index",
            .bits = INSTRUCTION_LITERAL(x010100010xxxxxxxxxxxxxxxxxxxxxx),
        },
        {
            .name = "STP",
            .mnemonic = "STP",
            .short_description = "Store pair of registers\n",
            .description = description,
            .variant_name = "Pre-index",
            .bits = INSTRUCTION_LITERAL(x010100110xxxxxxxxxxxxxxxxxxxxxx),
        },
        {
            .name = "STP",
            .mnemonic = "STP",
            .short_description = "Store pair of registers\n",
            .description = description,
            .variant_name = "Signed offset",
            .bits = INSTRUCTION_LITERAL(x010100100xxxxxxxxxxxxxxxxxxxxxx),
        },
    };

    test_single_file(XML_PATH("stp_gen.xml"), allocators, 3, instructions);
}

static void test_ldr_literal(instruction_allocators* allocators) {
    instruction instruction = {
        .name = "LDR (literal)",
        .mnemonic = "LDR",
        .short_description = "Load register (literal)\n",
        .description = "This instruction calculates an address from the PC value and\n"
                       "an immediate offset, loads a word from memory, and writes it to a\n"
                       "register. For information about addressing modes, see\n"
                       "<xref linkend=\"CHDIIIBB\">Load/Store addressing modes</xref>.\n",
        .variant_name = "Literal",
        .bits = INSTRUCTION_LITERAL(0x011000xxxxxxxxxxxxxxxxxxxxxxxx),
    };

    test_single_file(XML_PATH("ldr_lit_gen.xml"), allocators, 1, &instruction);
}

static void test_adrp(instruction_allocators* allocators) {
    instruction instruction = {
        .name = "ADRP",
        .mnemonic = "ADRP",
        .short_description = "Form PC-relative address to 4KB page\n",
        .description = "This instruction adds an immediate value that is\n"
                       "shifted left by 12 bits, to the PC value to form a PC-relative address,\n"
                       "with the bottom 12 bits masked out, and writes the result to the\n"
                       "destination register.\n",
        .variant_name = "Literal",
        .bits = INSTRUCTION_LITERAL(1xx10000xxxxxxxxxxxxxxxxxxxxxxxx),
    };

    test_single_file(XML_PATH("adrp.xml"), allocators, 1, &instruction);
}

static void test_ldbfmaxnm(instruction_allocators* allocators) {
    char* description = "This instruction atomically loads a 16-bit value from memory,\n"
                        "calculates the BFloat16 maximum number with the value held in a register,\n"
                        "and stores the result back to memory.\n"
                        "The value initially loaded from memory is returned in the destination register.\n"
                        "This instruction:\n"
                        "For more information about memory ordering semantics, see <xref linkend=\"BEIHCHEF\">Load-Acquire, Store-Release</xref>.\n"
                        "For information about addressing modes, see\n"
                        "<xref linkend=\"CHDIIIBB\">Load/Store addressing modes</xref>.\n";
    // xxxxxxxxxx000110xxxxx1xx00111100
    // xxxxxxxxxx000110xxxxx1xx00111100
    // xxxxxxxxxx000110xxxxx1xx00111100
    // xxxxxxxxxx000110xxxxx1xx00111100
    instruction instructions[] = {
        {
            .name = "LDBFMAXNM, LDBFMAXNMA, LDBFMAXNMAL, LDBFMAXNML",
            .mnemonic = "LDBFMAXNM",
            .short_description = "Atomic BFloat16 maximum number\n",
            .description = description,
            .variant_name = "Floating-point",
            .bits = INSTRUCTION_LITERAL(00111100001xxxxx011000xxxxxxxxxx),
        },
        {
            .name = "LDBFMAXNM, LDBFMAXNMA, LDBFMAXNMAL, LDBFMAXNML",
            .mnemonic = "LDBFMAXNMA",
            .short_description = "Atomic BFloat16 maximum number\n",
            .description = description,
            .variant_name = "Floating-point",
            .bits = INSTRUCTION_LITERAL(00111100101xxxxx011000xxxxxxxxxx),
        },
        {
            .name = "LDBFMAXNM, LDBFMAXNMA, LDBFMAXNMAL, LDBFMAXNML",
            .mnemonic = "LDBFMAXNMAL",
            .short_description = "Atomic BFloat16 maximum number\n",
            .description = description,
            .variant_name = "Floating-point",
            .bits = INSTRUCTION_LITERAL(00111100111xxxxx011000xxxxxxxxxx),
        },
        {
            .name = "LDBFMAXNM, LDBFMAXNMA, LDBFMAXNMAL, LDBFMAXNML",
            .mnemonic = "LDBFMAXNML",
            .short_description = "Atomic BFloat16 maximum number\n",
            .description = description,
            .variant_name = "Floating-point",
            .bits = INSTRUCTION_LITERAL(00111100011xxxxx011000xxxxxxxxxx),
        },
    };

    test_single_file(XML_PATH("ldbfmaxnm.xml"), allocators, 4, instructions);
}

static void test_ldadd(instruction_allocators* allocators) {
    char* description = "This instruction\n"
                        "atomically loads a 32-bit word or 64-bit doubleword from memory, adds the value\n"
                        "held in a register to it, and stores the result back to memory.\n"
                        "The value initially loaded from memory is returned in the destination register.\n"
                        "For more information about memory ordering semantics, see <xref linkend=\"BEIHCHEF\">Load-Acquire, Store-Release</xref>.\n"
                        "For information about addressing modes, see\n"
                        "<xref linkend=\"CHDIIIBB\">Load/Store addressing modes</xref>.\n";

    instruction instructions[] = {
        {
            .name = "LDADD, LDADDA, LDADDAL, LDADDL",
            .mnemonic = "LDADD",
            .short_description = "Atomic add on word or doubleword\n",
            .description = description,
            .variant_name = "Integer",
            .bits = INSTRUCTION_LITERAL(10111000001xxxxx000000xxxxxxxxxx),
        },
        {
            .name = "LDADD, LDADDA, LDADDAL, LDADDL",
            .mnemonic = "LDADDA",
            .short_description = "Atomic add on word or doubleword\n",
            .description = description,
            .variant_name = "Integer",
            .bits = INSTRUCTION_LITERAL(10111000101xxxxx000000xxxxxxxxxx),
        },
        {
            .name = "LDADD, LDADDA, LDADDAL, LDADDL",
            .mnemonic = "LDADDAL",
            .short_description = "Atomic add on word or doubleword\n",
            .description = description,
            .variant_name = "Integer",
            .bits = INSTRUCTION_LITERAL(10111000111xxxxx000000xxxxxxxxxx),
        },
        {
            .name = "LDADD, LDADDA, LDADDAL, LDADDL",
            .mnemonic = "LDADDL",
            .short_description = "Atomic add on word or doubleword\n",
            .description = description,
            .variant_name = "Integer",
            .bits = INSTRUCTION_LITERAL(10111000011xxxxx000000xxxxxxxxxx),
        },
        {
            .name = "LDADD, LDADDA, LDADDAL, LDADDL",
            .mnemonic = "LDADD",
            .short_description = "Atomic add on word or doubleword\n",
            .description = description,
            .variant_name = "Integer",
            .bits = INSTRUCTION_LITERAL(11111000001xxxxx000000xxxxxxxxxx),
        },
        {
            .name = "LDADD, LDADDA, LDADDAL, LDADDL",
            .mnemonic = "LDADDA",
            .short_description = "Atomic add on word or doubleword\n",
            .description = description,
            .variant_name = "Integer",
            .bits = INSTRUCTION_LITERAL(11111000101xxxxx000000xxxxxxxxxx),
        },
        {
            .name = "LDADD, LDADDA, LDADDAL, LDADDL",
            .mnemonic = "LDADDAL",
            .short_description = "Atomic add on word or doubleword\n",
            .description = description,
            .variant_name = "Integer",
            .bits = INSTRUCTION_LITERAL(11111000111xxxxx000000xxxxxxxxxx),
        },
        {
            .name = "LDADD, LDADDA, LDADDAL, LDADDL",
            .mnemonic = "LDADDL",
            .short_description = "Atomic add on word or doubleword\n",
            .description = description,
            .variant_name = "Integer",
            .bits = INSTRUCTION_LITERAL(11111000011xxxxx000000xxxxxxxxxx),
        },
    };

    test_single_file(XML_PATH("ldadd.xml"), allocators, 8, instructions);
}

static void test_setptn(instruction_allocators* allocators) {
    char* description = "These instructions set a required number of bytes in memory to the value in the least significant\n"
                        "byte of the source data register. The prologue, main, and epilogue instructions are expected to\n"
                        "be run in succession and to appear consecutively in memory: SETPTN, then SETMTN,\n"
                        "and then SETETN.\n"
                        "SETPTN performs some preconditioning of the arguments suitable for using the\n"
                        "SETMTN instruction, and sets an <arm-defined-word>IMPLEMENTATION DEFINED</arm-defined-word> portion of the requested number\n"
                        "of bytes. SETMTN sets a further <arm-defined-word>IMPLEMENTATION DEFINED</arm-defined-word> portion of the remaining bytes.\n"
                        "SETETN sets any final remaining bytes.\n"
                        "The ability to set an <arm-defined-word>IMPLEMENTATION DEFINED</arm-defined-word> number of bytes allows an implementation to\n"
                        "optimize how the bytes being set are divided between the different instructions.\n"
                        "For more information on exceptions specific to memory set instructions,\n"
                        "see <xref linkend=\"MDSec.memcpy_and_memset_exceptions\">Memory Copy and Memory Set exceptions</xref>.\n"
                        "The architecture supports two algorithms for the memory set: option A and option B. Which\n"
                        "algorithm is used is <arm-defined-word>IMPLEMENTATION DEFINED</arm-defined-word>.\n"
                        "Portable software should not assume that the choice of algorithm is constant.\n"
                        "For SETPTN:\n"
                        "On completion of SETPTN, option A:\n"
                        "On completion of SETPTN, option B:\n"
                        "For SETMTN, option A, when PSTATE.C = '0':\n"
                        "For SETMTN, option B, when PSTATE.C = '1':\n"
                        "For SETETN, option A, when PSTATE.C = '0':\n"
                        "For SETETN, option B, when PSTATE.C = '1':\n"
                        "Explicit Memory Write  effects produced by the instruction behave as if the instruction was\n"
                        "  executed at EL0 if the <xref linkend=\"Effective_value\">Effective value</xref> of\n"
                        "  PSTATE.UAO is 0 and either:\n"
                        "Otherwise, the Explicit Memory Write  effects operate with the restrictions determined by\n"
                        "  the Exception level at which the instruction is executed.\n";

    instruction instructions[] = {
        {
            .name = "SETPTN, SETMTN, SETETN",
            .mnemonic = "SETPTN",
            .short_description = "Memory set, unprivileged and non-temporal\n",
            .description = description,
            .variant_name = "Integer",
            .bits = INSTRUCTION_LITERAL(xx011001110xxxxx001101xxxxxxxxxx),
        },
        {
            .name = "SETPTN, SETMTN, SETETN",
            .mnemonic = "SETMTN",
            .short_description = "Memory set, unprivileged and non-temporal\n",
            .description = description,
            .variant_name = "Integer",
            .bits = INSTRUCTION_LITERAL(xx011001110xxxxx011101xxxxxxxxxx),
        },
        {
            .name = "SETPTN, SETMTN, SETETN",
            .mnemonic = "SETETN",
            .short_description = "Memory set, unprivileged and non-temporal\n",
            .description = description,
            .variant_name = "Integer",
            .bits = INSTRUCTION_LITERAL(xx011001110xxxxx101101xxxxxxxxxx),
        },
    };

    test_single_file(XML_PATH("setptn.xml"), allocators, 3, instructions);
}

static void parse_all(instruction_allocators* allocators) {
    glob_t files;
    EXPECT(glob(XML_PATH("*.xml"), GLOB_ERR | GLOB_NOSORT, NULL, &files) == 0);

    for (size_t i = 0; i < files.gl_pathc; i++) {
        printf(" - %s\n", files.gl_pathv[i]);

        instruction* instructions = allocate_bytes(allocators->instruction_allocator, 0, 32);
        u8 instruction_count = parse_instructions(files.gl_pathv[i], *allocators);

        EXPECT(instruction_count > 0);
        for (int j = 0; j < instruction_count; j++) {
            EXPECT(strlen(instructions[j].mnemonic) < 12);

            for (int b = 0; b < 32; b++) {
                EXPECT(instructions[j].bits[b] == '1' || instructions[j].bits[b] == '0' || instructions[j].bits[b] == 'x');
            }
        }

    }
}

int run_instruction_set_parser_tests(void) {
    instruction_allocators allocators;
    virtual_arena* general_allocator = create_arena(GB(1));
    allocators.general_allocator = general_allocator;
    allocators.bitpattern_allocator = create_arena(GB(1));
    allocators.instruction_allocator = create_arena(GB(1));

    prepare_tests("instruction_set_parser_tests");

    run_instruction_test(test_add_immediate, &allocators, "Test add immediate");
    run_instruction_test(test_movz, &allocators, "Test movz");
    run_instruction_test(test_movk, &allocators, "Test movk");
    run_instruction_test(test_b_uncond, &allocators, "Test b");
    run_instruction_test(test_ret, &allocators, "Test ret");
    run_instruction_test(test_adr, &allocators, "Test adr");
    run_instruction_test(test_b_cond, &allocators, "Test b.cond");
    run_instruction_test(test_tbz, &allocators, "Test tbz");
    run_instruction_test(test_add_shifted_register, &allocators, "Test add shifted register");
    run_instruction_test(test_add_extended_register, &allocators, "Test add extended register");
    run_instruction_test(test_nop, &allocators, "Test nop");
    run_instruction_test(test_dsb, &allocators, "Test dsb");
    run_instruction_test(test_ldr_immediate, &allocators, "Test ldr immediate");
    run_instruction_test(test_ldapr, &allocators, "Test ldapr");
    run_instruction_test(test_mov_register, &allocators, "Test mov register");
    run_instruction_test(test_bl, &allocators, "Test bl");
    run_instruction_test(test_mov_bitmask_immediate, &allocators, "Test mov bitmask immediate");
    run_instruction_test(test_str_immediate, &allocators, "Test str immediate");
    run_instruction_test(test_cbz, &allocators, "Test cbz");
    run_instruction_test(test_cmp_immediate, &allocators, "Test cmp immediate");
    run_instruction_test(test_ldp, &allocators, "Test ldp");
    run_instruction_test(test_stp, &allocators, "Test stp");
    run_instruction_test(test_ldr_literal, &allocators, "Test ldr literal");
    run_instruction_test(test_adrp, &allocators, "Test adrp");
    run_instruction_test(test_ldbfmaxnm, &allocators, "Test ldbfmaxnm");
    run_instruction_test(test_ldadd, &allocators, "Test ldadd");
    run_instruction_test(test_setptn, &allocators, "Test setptn");

    run_instruction_test(parse_all, &allocators, "Parse all");

    printf("%llu passed, %llu failed\n", g_passed, g_failed);

    destroy_arena(allocators.bitpattern_allocator);
    destroy_arena(allocators.instruction_allocator);
    destroy_arena(allocators.general_allocator);

    return g_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
