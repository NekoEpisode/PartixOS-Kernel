#include "include/paging.h"

void paging_load_satp(uint64_t satp) {
    __asm__ volatile("csrw satp, %0" :: "r"(satp));
    __asm__ volatile("sfence.vma");
}

uint64_t paging_read_stval(void) {
    uint64_t val;
    __asm__ volatile("csrr %0, stval" : "=r"(val));
    return val;
}
