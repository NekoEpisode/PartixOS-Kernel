// Partix heap allocator — size-class (slab-style) design to avoid
// fragmentation for Partic's allocation pattern (every `new` is a heap
// allocation, objects are freed manually with Memory.free).
//
// Layout per allocation (16-byte alignment throughout):
//   [ 16-byte header | object data ... ]
//     header[0] = size-class index, or BIG_MAGIC for large allocations
//     header[1] = page count for large allocations
//   kr_alloc returns the object address (header + 16), which the Partic
//   compiler uses directly (vtable at offset 0, fields after).
//
// Small objects (<= SLAB_MAX) come from per-size-class free lists; blocks of
// the same class are interchangeable, so there is no external fragmentation —
// a freed block is always reused by a same-class allocation. Large objects
// (> SLAB_MAX) get whole pages, so they cannot fragment the small pools.
//
// First version: the page region is carved with a bump pointer; large-page
// reclamation is left for a later bitmap-based pass (see TODO).

#include "allocator.h"
#include "page_alloc.h"

#define HEADER_SIZE   16
#define PAGE_SIZE     4096
#define SLAB_MAX      2048
#define BIG_MAGIC     0xFFFF

// Size classes: +8 up to 64, then +16/+32/+64/+128/+256 up to 2048.
static const uint32_t class_sizes[] = {
    8, 16, 24, 32, 40, 48, 56, 64,
    80, 96, 112, 128, 160, 192, 224, 256,
    320, 384, 448, 512, 640, 768, 896, 1024,
    1280, 1536, 1792, 2048
};
#define CLASS_COUNT ((int)(sizeof(class_sizes) / sizeof(class_sizes[0])))

static uint64_t free_lists[CLASS_COUNT];  // head block address per class
static uint64_t page_start;               // first page of the heap region
static uint64_t page_next;                // next free page offset
static uint64_t page_end;
static int inited;

// counters for stats / leak verification
static uint64_t small_in_use_blocks;
static uint64_t big_outstanding;

static uint64_t align16(uint64_t v) { return (v + 15) & ~15ULL; }

static void alloc_init(void) {
    if (inited) return;
    page_start = align16(kr_heap_start);
    page_next = page_start;
    page_end = kr_heap_end;
    for (int i = 0; i < CLASS_COUNT; i++) free_lists[i] = 0;
    inited = 1;
}

// Smallest class index whose block is >= size; -1 for large.
static int class_of(uint64_t size) {
    if (size > SLAB_MAX) return -1;
    for (int i = 0; i < CLASS_COUNT; i++) {
        if (class_sizes[i] >= size) return i;
    }
    return -1;
}

static uint64_t slab_block_size(int ci) {
    return align16(HEADER_SIZE + class_sizes[ci]);
}

// Extension pages from the page allocator (once the bootstrap bump region
// is exhausted): one page at a time, carved in-page for slab classes.
static uint64_t ext_page;      // current extension page base
static uint64_t ext_page_off;  // used offset within the page

// Carve one new block for class ci. Prefers the bootstrap bump region;
// after it is exhausted, takes pages from the page allocator.
static uint64_t slab_carve(int ci) {
    uint64_t need = slab_block_size(ci);
    if (page_next + need <= page_end) {
        uint64_t block = page_next;
        page_next += need;
        return block;
    }
    if (!ext_page || ext_page_off + need > PAGE_SIZE) {
        ext_page = page_alloc();
        if (!ext_page) return 0;  // heap exhausted
        ext_page_off = 0;
    }
    uint64_t block = ext_page + ext_page_off;
    ext_page_off += need;
    return block;
}

uint64_t kr_alloc(uint64_t size) {
    alloc_init();
    if (size < 8) size = 8;

    if (size <= SLAB_MAX) {
        int ci = class_of(size);
        uint64_t block = free_lists[ci];
        if (block) {
            free_lists[ci] = *(uint64_t *)block;   // pop from free list
        } else {
            block = slab_carve(ci);
            if (!block) return 0;
        }
        ((uint64_t *)block)[0] = (uint64_t)ci;
        small_in_use_blocks++;
        return block + HEADER_SIZE;
    }

    // Large allocation: whole pages. Prefers the bootstrap bump region;
    // after it is exhausted, takes contiguous pages from the page allocator.
    uint64_t need = align16(HEADER_SIZE + size);
    uint64_t pages = (need + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t block;
    if (page_next + pages * PAGE_SIZE <= page_end) {
        block = page_next;
        page_next += pages * PAGE_SIZE;
    } else {
        block = page_alloc_contig((int)pages);
        if (!block) return 0;
    }
    ((uint64_t *)block)[0] = BIG_MAGIC;
    ((uint64_t *)block)[1] = pages;
    big_outstanding++;
    return block + HEADER_SIZE;
}

void kr_dealloc(uint64_t addr) {
    if (!addr) return;
    uint64_t block = addr - HEADER_SIZE;
    uint64_t ci = ((uint64_t *)block)[0];

    if (ci == BIG_MAGIC) {
        uint64_t pages = ((uint64_t *)block)[1];
        // Bump-region pages are reserved in the page allocator, so
        // page_free on them is a safe no-op; extension pages are returned.
        for (uint64_t i = 0; i < pages; i++) page_free(block + i * PAGE_SIZE);
        big_outstanding--;
        return;
    }
    if (ci >= (uint64_t)CLASS_COUNT) return;   // corrupt header — ignore

    *(uint64_t *)block = free_lists[ci];       // push onto free list
    free_lists[ci] = block;
    small_in_use_blocks--;
}

void kr_alloc_stats(kr_alloc_stats_t *out) {
    alloc_init();
    uint64_t nfree = 0;
    for (int i = 0; i < CLASS_COUNT; i++) {
        uint64_t b = free_lists[i];
        while (b) { nfree++; b = *(uint64_t *)b; }
    }
    out->pages_used = (page_next - page_start) / PAGE_SIZE;
    out->small_blocks = small_in_use_blocks;
    out->small_free = nfree;
    out->big_blocks = big_outstanding;
}

uint64_t kr_alloc_pages_used(void) {
    alloc_init();
    return (page_next - page_start) / PAGE_SIZE;
}

uint64_t kr_alloc_small_in_use(void) {
    alloc_init();
    return small_in_use_blocks;
}
