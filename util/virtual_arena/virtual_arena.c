#include <sys/mman.h>
#include <unistd.h>

#include "virtual_arena.h"
#include "../preamble.h"

static size_t get_page_count(size_t size) {
    return (size + PAGE_SIZE - 1) / PAGE_SIZE;
}

/**
 * Commits enough pages to hold the requested number of bytes.
 *
 * @param page_start The start of the first page to commit
 * @param bytes The minimum number of bytes needed
 * @return The start of the next page after what's been committed.
 */
static void* commit_enough_pages_for(void* page_start, const size_t bytes) {
    const size_t pages_needed = get_page_count(bytes);

    // TODO: I guess this is bad and unnecessary, the kernel will do this for us more efficiently.
    // Maybe should MADV_WILLNEED ahead of time instead
    if (mprotect(page_start, PAGE_SIZE * pages_needed, PROT_READ | PROT_WRITE) == -1) {
        return NULL;
    }

    return page_start + (PAGE_SIZE * pages_needed);
}

virtual_arena* create_arena(const size_t requested_size) {
    const size_t size = get_page_count(requested_size) * PAGE_SIZE;

    void* reservation_start = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (reservation_start == MAP_FAILED) {
        return NULL;
    }

    // Commit enough space for our metadata struct
    void* next_page_address = commit_enough_pages_for(reservation_start, sizeof(virtual_arena));
    if (next_page_address == NULL) {
        munmap(reservation_start, size);
        return NULL;
    }

    virtual_arena* arena = reservation_start;
    arena->current_address = reservation_start + sizeof(virtual_arena);
    arena->end_address = reservation_start + size;
    arena->next_page_address = next_page_address;

    return arena;
}

void* allocate_bytes(virtual_arena* arena, const size_t size, const size_t alignment) {
    void* granted_address = (void*)align_up((uptr)arena->current_address, alignment);
    void* allocation_end = granted_address + size;
    if (allocation_end > arena->end_address) {
        return NULL;
    }

    if (allocation_end > arena->next_page_address) {
        const size_t bytes_past_frontier = allocation_end - arena->next_page_address;

        void* next_page_address = commit_enough_pages_for(arena->next_page_address, bytes_past_frontier);
        if (next_page_address == NULL) {
            return NULL;
        }
        arena->next_page_address = next_page_address;
    }

    arena->current_address = allocation_end;

    return granted_address;
}

void destroy_arena(virtual_arena* arena) {
    munmap(arena, (uptr)arena->end_address - (uptr)arena);
}
