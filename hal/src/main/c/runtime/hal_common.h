#ifndef HAL_COMMON_H
#define HAL_COMMON_H

#include <stdint.h>

// ── x86_64 paging ──────────────────
void paging_load_cr3(uint64_t cr3);
uint64_t paging_read_cr2(void);

// ── RISC-V paging ──────────────────
void paging_load_satp(uint64_t satp);
uint64_t paging_read_stval(void);

// ── FDT (RISC-V) ───────────────────
uint64_t fdt_get_uart_base(void);
uint64_t fdt_get_pcie_base(void);
uint64_t fdt_get_plic_base(void);

#endif
