#include <stdint.h>
#include "include/fdt.h"

uint32_t* gop_framebuffer;
uint64_t gop_width, gop_height, gop_stride;
uint64_t uefi_mmap_addr, uefi_mmap_size, uefi_mmap_desc_size;
void*    fdt_addr = 0;

volatile unsigned long g_tick;
long archId = 1;

typedef struct {
    void*    framebuffer;
    uint64_t width, height, stride, format;
    uint64_t memoryMap, memoryMapSize, memoryMapDescriptorSize;
    void*    fdt_addr;
} BootInfo;

// Called from entry.S trap_entry
// Symbol in .ll: kr_partix_kernel_interrupt_InterruptBridge_dispatch__IJJJV
extern void kr_partix_kernel_interrupt_InterruptBridge_dispatch__IJJJV(
    int cause, uint64_t epc, uint64_t sp, uint64_t frame);

void trap_dispatch(uint64_t cause, uint64_t epc, void* frame) {
    uint64_t sp;
    __asm__ volatile("mv %0, sp" : "=r"(sp));
    kr_partix_kernel_interrupt_InterruptBridge_dispatch__IJJJV(
        (int)cause, epc, sp, (uint64_t)frame);
}

void kernel_entry(BootInfo* info) {
    if (info) {
        gop_framebuffer = info->framebuffer;
        gop_width  = info->width;
        gop_height = info->height;
        gop_stride = info->stride;
        uefi_mmap_addr = info->memoryMap;
        uefi_mmap_size = info->memoryMapSize;
        uefi_mmap_desc_size = info->memoryMapDescriptorSize;
        fdt_addr = info->fdt_addr;
    }

    extern void _start(void);
    fdt_init(fdt_addr);
    _start();

    while (1) __asm__ volatile("wfi");
}
