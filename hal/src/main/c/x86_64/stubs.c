// Unused cross-arch symbols — only needed because
// Partic compiles all arch classes together and the
// linker needs every extern resolved.

#include "../runtime/hal_common.h"

void paging_load_satp(uint64_t satp) { (void)satp; }
uint64_t paging_read_stval(void) { return 0; }
uint64_t fdt_get_base(void) { return 0; }
uint64_t fdt_get_uart_base(void) { return 0; }
uint64_t fdt_get_uart_reg_shift(void) { return 0; }   // RISC-V only; link stub
uint64_t fdt_get_pcie_base(void) { return 0; }
uint64_t fdt_get_plic_base(void) { return 0; }
void sbi_console_putchar(int c) { (void)c; }   // RISC-V only; link stub
