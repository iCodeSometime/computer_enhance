#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <sys/mman.h>

#include "util/preamble.h"
#include "tests.h"
#include "../virtual_arena/virtual_arena.h"

static void expect_aligned(void* p, size_t alignment) {
    EXPECT(p != NULL);
    EXPECT((uintptr_t)p % alignment == 0);
}

/* The arena object is the base of the mmap reservation. */
static void release_arena(virtual_arena* arena, size_t size) {
    if (arena != NULL) {
        munmap(arena, size);
    }
}

static void test_create_and_reject_empty(void) {
    virtual_arena* arena = create_arena(PAGE_SIZE);
    EXPECT(arena != NULL);
    release_arena(arena, PAGE_SIZE);

    EXPECT(create_arena(0) == NULL);
}

static void test_bump_is_contiguous(void) {
    const size_t reserve = PAGE_SIZE;
    virtual_arena* arena = create_arena(reserve);
    EXPECT(arena != NULL);

    void* first = allocate_bytes(arena, 24, 16);
    void* second = allocate_bytes(arena, 8, 8);
    EXPECT(first != NULL);
    EXPECT(second != NULL);
    EXPECT((unsigned char*)second == (unsigned char*)first + 24);
    EXPECT((uintptr_t)first % sizeof(void*) == 0);

    release_arena(arena, reserve);
}

static void test_bytes_round_trip_across_pages(void) {
    const size_t reserve = PAGE_SIZE * 2;
    virtual_arena* arena = create_arena(reserve);
    EXPECT(arena != NULL);

    const size_t start = align_up(sizeof(virtual_arena), 16);
    const size_t usable = reserve - start;
    unsigned char* block = allocate_bytes(arena, usable, 16);
    EXPECT(block != NULL);

    for (size_t i = 0; i < usable; i++) {
        block[i] = (unsigned char)(i * 17u);
    }
    for (size_t i = 0; i < usable; i++) {
        EXPECT(block[i] == (unsigned char)(i * 17u));
    }

    EXPECT(allocate_bytes(arena, 1, 1) == NULL);
    release_arena(arena, reserve);
}

static void test_one_byte_at_a_time_crosses_boundary(void) {
    const size_t reserve = PAGE_SIZE * 2;
    virtual_arena* arena = create_arena(reserve);
    EXPECT(arena != NULL);

    const size_t usable = reserve - sizeof(virtual_arena);
    const unsigned char* first = NULL;
    for (size_t i = 0; i < usable; i++) {
        unsigned char* p = allocate_bytes(arena, 1, 1);
        EXPECT(p != NULL);
        if (i == 0) {
            first = p;
        }
        EXPECT(p == first + i);
        *p = (unsigned char)i;
    }
    EXPECT(allocate_bytes(arena, 1, 1) == NULL);

    for (size_t i = 0; i < usable; i++) {
        EXPECT(first[i] == (unsigned char)i);
    }
    release_arena(arena, reserve);
}

static void test_single_alloc_spans_pages(void) {
    const size_t reserve = PAGE_SIZE * 3;
    virtual_arena* arena = create_arena(reserve);
    EXPECT(arena != NULL);

    const size_t bytes = PAGE_SIZE * 2;
    unsigned char* block = allocate_bytes(arena, bytes, 16);
    EXPECT(block != NULL);
    block[0] = 0x11;
    block[PAGE_SIZE - 1] = 0x22;
    block[PAGE_SIZE] = 0x33;
    block[bytes - 1] = 0x44;
    EXPECT(block[0] == 0x11);
    EXPECT(block[PAGE_SIZE - 1] == 0x22);
    EXPECT(block[PAGE_SIZE] == 0x33);
    EXPECT(block[bytes - 1] == 0x44);

    release_arena(arena, reserve);
}

static void test_ends_exactly_on_page_boundary(void) {
    const size_t reserve = PAGE_SIZE;
    virtual_arena* arena = create_arena(reserve);
    EXPECT(arena != NULL);

    size_t to_boundary = PAGE_SIZE - align_up(sizeof(virtual_arena), 16);
    unsigned char* block = allocate_bytes(arena, to_boundary, 16);
    EXPECT(block != NULL);
    memset(block, 0xAB, to_boundary);
    EXPECT(block[0] == 0xAB);
    EXPECT(block[to_boundary - 1] == 0xAB);
    EXPECT(allocate_bytes(arena, 1, 1) == NULL);

    release_arena(arena, reserve);
}

static void test_failed_alloc_keeps_the_rest(void) {
    const size_t reserve = PAGE_SIZE;
    virtual_arena* arena = create_arena(reserve);
    EXPECT(arena != NULL);

    const size_t start = align_up(sizeof(virtual_arena), 16);
    const size_t remaining = reserve - start;
    const unsigned char* head = allocate_bytes(arena, remaining - 4, 16);
    EXPECT(head != NULL);
    EXPECT(allocate_bytes(arena, 8, 4) == NULL);

    unsigned char* tail = allocate_bytes(arena, 4, 4);
    EXPECT(tail != NULL);
    EXPECT(tail == head + (remaining - 4));
    memset(tail, 0x7E, 4);
    EXPECT(tail[0] == 0x7E);
    EXPECT(tail[3] == 0x7E);
    EXPECT(allocate_bytes(arena, 1, 1) == NULL);

    release_arena(arena, reserve);
}

static void test_zero_size_does_not_advance(void) {
    const size_t reserve = PAGE_SIZE;
    virtual_arena* arena = create_arena(reserve);
    EXPECT(arena != NULL);

    const void* a = allocate_bytes(arena, 0, 1);
    const void* b = allocate_bytes(arena, 0, 1);
    void* c = allocate_bytes(arena, 4, 4);
    EXPECT(a != NULL);
    EXPECT(a == b);
    EXPECT(c == a);
    memset(c, 0x5A, 4);
    EXPECT(((unsigned char*)c)[3] == 0x5A);

    release_arena(arena, reserve);
}

static void test_two_arenas_are_independent(void) {
    const size_t reserve = PAGE_SIZE;
    virtual_arena* a = create_arena(reserve);
    virtual_arena* b = create_arena(reserve);
    EXPECT(a != NULL);
    EXPECT(b != NULL);
    EXPECT(a != b);

    unsigned char* pa = allocate_bytes(a, 16, 16);
    unsigned char* pb = allocate_bytes(b, 16, 16);
    EXPECT(pa != NULL);
    EXPECT(pb != NULL);
    memset(pa, 0xA1, 16);
    memset(pb, 0xB2, 16);
    EXPECT(pa[0] == 0xA1);
    EXPECT(pb[15] == 0xB2);

    release_arena(a, reserve);
    release_arena(b, reserve);
}

static void test_u32_sequence_like_the_decoder(void) {
    const size_t count = 100000;
    const size_t bytes = count * sizeof(uint32_t);
    size_t reserve = PAGE_SIZE;
    while (reserve < bytes + align_up(sizeof(virtual_arena), 16)) {
        reserve += PAGE_SIZE;
    }

    virtual_arena* arena = create_arena(reserve);
    EXPECT(arena != NULL);

    uint32_t* values = allocate_bytes(arena, bytes, 16);
    EXPECT(values != NULL);
    for (uint32_t i = 0; i < count; i++) {
        values[i] = i;
    }
    EXPECT(values[0] == 0);
    EXPECT(values[PAGE_SIZE / sizeof(uint32_t)] == PAGE_SIZE / sizeof(uint32_t));
    EXPECT(values[count - 1] == count - 1);

    release_arena(arena, reserve);
}

static void test_fresh_allocations_are_aligned(void) {
    virtual_arena* arena = create_arena(PAGE_SIZE);
    EXPECT(arena != NULL);

    void* four = allocate_bytes(arena, 4, 4);
    void* eight = allocate_bytes(arena, 8, 8);
    void* sixteen = allocate_bytes(arena, 16, 16);

    expect_aligned(four, 4);
    expect_aligned(eight, 8);
    expect_aligned(sixteen, 16);

    release_arena(arena, PAGE_SIZE);
}

static void test_alignment_after_one_byte(void) {
    virtual_arena* arena = create_arena(PAGE_SIZE);
    EXPECT(arena != NULL);

    allocate_bytes(arena, 1, 1);

    void* four = allocate_bytes(arena, 4, 4);
    void* eight = allocate_bytes(arena, 8, 8);
    void* sixteen = allocate_bytes(arena, 16, 16);

    expect_aligned(four, 4);
    expect_aligned(eight, 8);
    expect_aligned(sixteen, 16);

    release_arena(arena, PAGE_SIZE);
}

static void test_unaligned_reservation_size(void) {
    const size_t requested = PAGE_SIZE + 100;
    const size_t reserve = PAGE_SIZE * 2;
    virtual_arena* arena = create_arena(requested);
    EXPECT(arena != NULL);

    const size_t start = align_up(sizeof(virtual_arena), 16);
    const size_t usable = reserve - start;
    unsigned char* block = allocate_bytes(arena, usable, 16);
    EXPECT(block != NULL);
    block[0] = 0x01;
    block[usable - 1] = 0x02;
    EXPECT(block[0] == 0x01);
    EXPECT(block[usable - 1] == 0x02);
    EXPECT(allocate_bytes(arena, 1, 1) == NULL);

    release_arena(arena, reserve);
}

int run_virtual_arena_tests(void) {
    prepare_tests("virtual_arena_tests");

    run_test(test_create_and_reject_empty, "create arena, reject zero size");
    run_test(test_bump_is_contiguous, "bump pointer is contiguous");
    run_test(test_ends_exactly_on_page_boundary, "fill exactly to the first page");
    run_test(test_bytes_round_trip_across_pages, "one block spanning two pages");
    run_test(test_one_byte_at_a_time_crosses_boundary, "one byte at a time across a boundary");
    run_test(test_single_alloc_spans_pages, "one allocation commits multiple pages");
    run_test(test_failed_alloc_keeps_the_rest, "a failed alloc leaves the rest usable");
    run_test(test_zero_size_does_not_advance, "zero-size alloc does not advance");
    run_test(test_two_arenas_are_independent, "two arenas do not alias");
    run_test(test_u32_sequence_like_the_decoder, "u32 fill across pages");
    run_test(test_unaligned_reservation_size, "reservation size not a multiple of the page");
    run_test(test_fresh_allocations_are_aligned, "fresh allocations are aligned to their size");
    run_test(test_alignment_after_one_byte, "alignment holds after a one-byte allocation");

    printf("%llu passed, %llu failed\n", g_passed, g_failed);
    return g_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
