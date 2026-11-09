#ifndef VIRTUAL_ARENA_H
#define VIRTUAL_ARENA_H

#include "util/preamble.h"
// aarch64-macos page size
#define PAGE_SIZE 16384

typedef struct virtual_arena {
    void* current_address;
    void* next_page_address;
    void* end_address;
} virtual_arena;

/**
 * Create a new arena with virtual memory of the given size.
 * Pages are committed as needed, so it doesn't take up the full memory unless it's used.
 * Since only full pages can be committed, the size will be rounded up to the nearest page boundary.
 *
 * @param size How big should the arena be - pages are only committed as needed.
 * @return The new arena.
 */
virtual_arena* create_arena(u64 size);

/**
 * Allocate some bytes in the arena.
 *
 * @param arena The arena to allocate bytes for.
 * @param size The number of bytes needed.
 * @param alignment The memory alignment for the start of the bytes needed - should be power of 2.
 *                  Typically, 2, 4, 8, 16. 64 puts it at the start of a cache-line.
 * @return A pointer to allocated bytes.
 */
void* allocate_bytes(virtual_arena* arena, u64 size, u64 alignment);
void destroy_arena(virtual_arena* arena);

#endif //VIRTUAL_ARENA_H
