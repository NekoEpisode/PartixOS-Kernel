#include "include/paging.h"

void paging_load_satp(uint64_t satp) {
    __asm__ volatile("csrw satp, %0" :: "r"(satp));
    __asm__ volatile("sfence.vma");
}

// 全量刷 TLB。必须在任何页表修改（map4k/unmap4k/map）之后调用：
// 只改内存中的 PTE 不够，TLB 残留旧翻译会让用户态访问误报 page fault。
void paging_flush_tlb(void) {
    __asm__ volatile("sfence.vma" ::: "memory");
}

uint64_t paging_read_stval(void) {
    uint64_t val;
    __asm__ volatile("csrr %0, stval" : "=r"(val));
    return val;
}
