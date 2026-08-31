#include "include/paging.h"

void paging_load_cr3(uint64_t cr3) {
    __asm__ volatile("mov %0, %%cr3" :: "r"(cr3) : "memory");
}

// 全量刷 TLB：重载 CR3（全局页除外）。必须在任何页表修改之后调用，
// 否则 TLB 里残留的旧翻译（如恒等映射的 2MB U/S=0 大页项）会让用户态
// 访问误报 #PF（与 riscv 的 paging_flush_tlb 同源问题）。
void paging_flush_tlb(void) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3) : : "memory");
    __asm__ volatile("mov %0, %%cr3" :: "r"(cr3) : "memory");
}

uint64_t paging_read_cr2(void) {
    uint64_t val;
    __asm__ volatile("mov %%cr2, %0" : "=r"(val));
    return val;
}
