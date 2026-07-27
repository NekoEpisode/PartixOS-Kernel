#include "include/idt.h"
#include "include/stdint.h"

uint32_t* gop_framebuffer;
uint64_t gop_width, gop_height, gop_stride;
uint64_t uefi_mmap_addr, uefi_mmap_size, uefi_mmap_desc_size;

volatile unsigned long g_tick;
volatile short archId = 0;

typedef struct {
    void*    framebuffer;
    uint64_t width, height, stride, format;
    uint64_t memoryMap, memoryMapSize, memoryMapDescriptorSize;
    uint64_t cs_selector;
} BootInfo;

// Called from idt.c default_handler — bridges interrupt to Partic
// Symbol: kr_partix_kernel_interrupt_InterruptBridge_dispatch__IJJJV
extern void kr_partix_kernel_interrupt_InterruptBridge_dispatch__IJJJV(
    int cause, uint64_t epc, uint64_t sp, uint64_t frame);

void kernel_entry(BootInfo* info) {
    gop_framebuffer = info->framebuffer;
    gop_width  = info->width;
    gop_height = info->height;
    gop_stride = info->stride;
    uefi_mmap_addr = info->memoryMap;
    uefi_mmap_size = info->memoryMapSize;
    uefi_mmap_desc_size = info->memoryMapDescriptorSize;

    asm volatile("cli");
    uint16_t cs = info->cs_selector;
    init_idt(cs);
    asm volatile("sti");

    extern void _start(void);
    _start();

    while (1) __asm__("hlt");
}
