#include <stdint.h>
#include "include/fdt.h"
#include "../runtime/mem_layout.h"

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

// Called from entry.S trap_entry. Returns the frame to restore from
// (same frame when no context switch happened).
// Symbol in .ll: P_kr_partix_kernel_interrupt_InterruptBridge.dispatch._1_1JJJJJ
// (mangled 名含 '.'，非合法 C 标识符，用 __asm__ 指定汇编符号名)
extern long kr_interrupt_bridge_dispatch(
    int64_t cause, uint64_t epc, uint64_t sp, uint64_t frame)
    __asm__("P_kr_partix_kernel_interrupt_InterruptBridge.dispatch._1_1JJJJJ");

long trap_dispatch(uint64_t cause, uint64_t epc, void* frame) {
    uint64_t sp;
    __asm__ volatile("mv %0, sp" : "=r"(sp));
    return kr_interrupt_bridge_dispatch(
        (int64_t)cause, epc, sp, (uint64_t)frame);
}

void kernel_entry(BootInfo* info) {
    // BootInfo 在 bootloader 栈上（物理地址）；经 physmap 映射访问
    // （引导表含 physmap，规范表亦然，映射一致）。
    if (info) {
        BootInfo* vi = (BootInfo*)(uintptr_t)phys_to_virt((uintptr_t)info);
        // 无 GOP 时 framebuffer 为 0，width/height/stride 不赋值（保持 0），
        // 防止 bootloader 栈垃圾传播到 gop_* 全局。
        if (vi->framebuffer) {
            gop_framebuffer = vi->framebuffer;
            gop_width  = vi->width;
            gop_height = vi->height;
            gop_stride = vi->stride;
        }
        uefi_mmap_addr = vi->memoryMap;
        uefi_mmap_size = vi->memoryMapSize;
        uefi_mmap_desc_size = vi->memoryMapDescriptorSize;
        fdt_addr = vi->fdt_addr;
    }

    extern void _start(void);
    fdt_init(fdt_addr);
    _start();

    while (1) __asm__ volatile("wfi");
}
