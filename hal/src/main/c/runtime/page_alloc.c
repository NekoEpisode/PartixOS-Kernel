// 4 KiB page allocator over the free physical memory (bitmap, first fit).
//
// The bitmap covers [span_start, span_start + span_pages*4096); pages
// outside the supplied free ranges (holes: reserved MMIO, UEFI runtime
// regions, ...) are marked occupied. The bitmap's own storage is carved
// from the head of the first free range and marked occupied.
//
// 物理页地址由接口对外返回（page_alloc/page_alloc_contig/page_free 的参数
// 与返回值均为物理地址）；bitmap 存储位于低地址物理页，须经 physmap 映射
// 为 VA 后访问。

#include "page_alloc.h"
#include <stdint.h>
#include "mem_layout.h"

#define PAGE_SIZE 4096

static uint8_t *bitmap;
static uint64_t span_start;
static uint64_t span_pages;
static uint64_t total_pages;
static uint64_t free_pages;
static int inited;

static inline int bit_get(uint64_t page) {
    return (bitmap[page >> 3] >> (page & 7)) & 1;
}

static inline void bit_set(uint64_t page) {
    bitmap[page >> 3] |= (uint8_t)(1u << (page & 7));
}

static inline void bit_clr(uint64_t page) {
    bitmap[page >> 3] &= (uint8_t)~(1u << (page & 7));
}

void page_alloc_init(const uint64_t *ranges, int count) {
    if (inited || !ranges || count <= 0) return;

    uint64_t min_start = ~0ULL, max_end = 0;
    for (int i = 0; i < count; i++) {
        uint64_t s = ranges[i * 2];
        uint64_t p = ranges[i * 2 + 1];
        if (p == 0) continue;
        if (s < min_start) min_start = s;
        uint64_t e = s + p * PAGE_SIZE;
        if (e > max_end) max_end = e;
    }
    if (min_start == ~0ULL) return;

    span_start = min_start;
    span_pages = (max_end - min_start) / PAGE_SIZE;

    // Bitmap storage: take it from the first range large enough to host it.
    // (The first range in mmap order may be tiny — e.g. x86's [0x0, 1 page].)
    uint64_t bm_bytes = (span_pages + 7) / 8;
    uint64_t bm_pages = (bm_bytes + PAGE_SIZE - 1) / PAGE_SIZE;
    int bm_idx = -1;
    for (int i = 0; i < count; i++) {
        if (ranges[i * 2 + 1] >= bm_pages) {
            bm_idx = i;
            break;
        }
    }
    if (bm_idx < 0) return;  // no range big enough for the bitmap
    // ranges 里存的是物理地址；bitmap 自身须经 physmap 映射访问
    bitmap = (uint8_t *)(uintptr_t)phys_to_virt(ranges[bm_idx * 2]);

    // Default occupied; clear the free ranges.
    for (uint64_t i = 0; i < bm_bytes; i++) bitmap[i] = 0xFF;
    for (int i = 0; i < count; i++) {
        uint64_t s = ranges[i * 2];
        uint64_t p = ranges[i * 2 + 1];
        uint64_t first = (s - span_start) / PAGE_SIZE;
        for (uint64_t j = 0; j < p; j++) bit_clr(first + j);
    }
    // Bitmap storage pages are occupied (relative to span_start).
    uint64_t bm_first = (ranges[bm_idx * 2] - span_start) / PAGE_SIZE;
    for (uint64_t i = 0; i < bm_pages; i++) bit_set(bm_first + i);

    total_pages = 0;
    free_pages = 0;
    for (int i = 0; i < count; i++) {
        uint64_t p = ranges[i * 2 + 1];
        total_pages += p;
        free_pages += p;
    }
    free_pages -= bm_pages;
    inited = 1;
}

void page_alloc_reserve(uint64_t start, uint64_t size) {
    if (!inited || size == 0) return;
    // Clamp to the bitmap span; the old form underflowed when
    // start < span_start (unsigned wrap marked everything used).
    uint64_t s = start < span_start ? span_start : start;
    uint64_t span_end = span_start + span_pages * PAGE_SIZE;
    uint64_t e = start + size;
    if (e > span_end) e = span_end;
    if (e <= s) return;
    uint64_t first = (s - span_start) / PAGE_SIZE;
    uint64_t last = (e - span_start + PAGE_SIZE - 1) / PAGE_SIZE;
    if (last > span_pages) last = span_pages;
    for (uint64_t p = first; p < last; p++) {
        if (!bit_get(p)) {
            bit_set(p);
            free_pages--;
        }
    }
}

uint64_t page_alloc(void) {
    if (!inited) return 0;
    for (uint64_t p = 0; p < span_pages; p++) {
        if (!bit_get(p)) {
            bit_set(p);
            free_pages--;
            return span_start + p * PAGE_SIZE;
        }
    }
    return 0;
}

uint64_t page_alloc_contig(int pages) {
    if (!inited || pages <= 0) return 0;
    uint64_t run = 0;
    for (uint64_t p = 0; p < span_pages; p++) {
        if (!bit_get(p)) {
            run++;
            if (run >= (uint64_t)pages) {
                uint64_t first = p + 1 - run;
                for (uint64_t j = first; j <= p; j++) bit_set(j);
                free_pages -= (uint64_t)pages;
                return span_start + first * PAGE_SIZE;
            }
        } else {
            run = 0;
        }
    }
    return 0;
}

void page_free(uint64_t addr) {
    if (!inited || addr < span_start) return;
    uint64_t p = (addr - span_start) / PAGE_SIZE;
    if (p >= span_pages) return;
    if (bit_get(p)) {
        bit_clr(p);
        free_pages++;
    }
}

uint64_t page_alloc_total_pages(void) { return total_pages; }
uint64_t page_alloc_free_pages(void) { return free_pages; }
