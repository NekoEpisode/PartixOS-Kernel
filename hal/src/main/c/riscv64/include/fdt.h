#ifndef FDT_H
#define FDT_H

#include <stdint.h>

#define FDT_MAGIC     0xd00dfeed
#define FDT_BEGIN_NODE 0x00000001
#define FDT_END_NODE   0x00000002
#define FDT_PROP       0x00000003
#define FDT_NOP        0x00000004
#define FDT_END        0x00000009

typedef struct {
    uint32_t magic;
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
} __attribute__((packed)) fdt_header_t;

void fdt_init(void *fdt_ptr);
uint64_t fdt_get_uart_base(void);
uint64_t fdt_get_plic_base(void);
uint64_t fdt_get_pcie_base(void);
uint64_t fdt_get_timebase_freq(void);

#endif
