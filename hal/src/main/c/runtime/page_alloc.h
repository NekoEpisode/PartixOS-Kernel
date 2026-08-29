#ifndef PAGE_ALLOC_H
#define PAGE_ALLOC_H

#include <stdint.h>

// 4 KiB page allocator (bitmap) for the whole free physical memory.
// Physical memory is identity-mapped on both targets, so a returned page
// address is directly usable.

// ranges: array of [start, page_count] pairs (physical), from the UEFI
// memory map free ranges. The bitmap itself is carved from the head of the
// first range.
void page_alloc_init(const uint64_t *ranges, int count);

// Mark [start, start+size) as occupied (kernel image, bootstrap heap, ...).
void page_alloc_reserve(uint64_t start, uint64_t size);

// Allocate one page; 0 on exhaustion.
uint64_t page_alloc(void);

// Allocate `pages` physically contiguous pages (first fit); 0 on failure.
uint64_t page_alloc_contig(int pages);

void page_free(uint64_t addr);

uint64_t page_alloc_total_pages(void);
uint64_t page_alloc_free_pages(void);

#endif
