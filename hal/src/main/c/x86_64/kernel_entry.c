#include <stdint.h>

uint32_t* gop_framebuffer;
uint64_t gop_width, gop_height, gop_stride;
uint64_t uefi_mmap_addr, uefi_mmap_size, uefi_mmap_desc_size;

volatile unsigned long g_tick;
volatile long archId = 0;

typedef struct {
    void*    framebuffer;
    uint64_t width, height, stride, format;
    uint64_t memoryMap, memoryMapSize, memoryMapDescriptorSize;
} BootInfo;

void kernel_entry(BootInfo* info) {
    gop_framebuffer = info->framebuffer;
    gop_width  = info->width;
    gop_height = info->height;
    gop_stride = info->stride;
    uefi_mmap_addr = info->memoryMap;
    uefi_mmap_size = info->memoryMapSize;
    uefi_mmap_desc_size = info->memoryMapDescriptorSize;

    extern void _start(void);
    _start();

    while (1) __asm__("hlt");
}
