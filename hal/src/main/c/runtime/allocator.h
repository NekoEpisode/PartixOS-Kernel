#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <stdint.h>

// Heap region provided by each architecture (see x86_64/runtime.c,
// riscv64/runtime.c). kr_heap_start must be 16-byte aligned.
extern uint64_t kr_heap_start;
extern uint64_t kr_heap_end;

// Allocate `size` bytes, 16-byte aligned. Returns 0 when the heap is
// exhausted. Every object carries a 16-byte header *before* the returned
// address so kr_dealloc can find its size class.
uint64_t kr_alloc(uint64_t size);

// Free a pointer previously returned by kr_alloc (must not be 0).
void kr_dealloc(uint64_t addr);

typedef struct {
    uint64_t pages_used;     // pages carved out of the heap region
    uint64_t small_blocks;   // small-object blocks currently in use
    uint64_t small_free;     // small-object blocks in the free lists
    uint64_t big_blocks;     // large allocations currently outstanding
} kr_alloc_stats_t;

void kr_alloc_stats(kr_alloc_stats_t *out);

// Convenience counters for runtime observability (Partic side).
uint64_t kr_alloc_pages_used(void);      // pages carved from the heap region
uint64_t kr_alloc_small_in_use(void);    // small-object blocks in use

#endif
