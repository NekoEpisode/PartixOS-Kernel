#include "include/idt.h"
#include "include/io.h"
#include "include/stdint.h"
#include "../runtime/mem_layout.h"

// Own flat GDT + TSS (replaces the UEFI one; kernel CS becomes 0x08).
extern void init_gdt(void);
// syscall instruction + NX enable (must run after GDT, before user mode).
extern void init_syscall(void);

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

// Called from isr.s common handler — bridges interrupt to Partic.
// Returns the frame to restore from (x86 ignores it until the scheduler lands).
// Symbol: P_kr_partix_kernel_interrupt_InterruptBridge.dispatch._1_1JJJJJ
// (mangled 名含 '.'，非合法 C 标识符，用 __asm__ 指定汇编符号名)
extern long kr_interrupt_bridge_dispatch(
    int64_t cause, uint64_t epc, uint64_t sp, uint64_t frame)
    __asm__("P_kr_partix_kernel_interrupt_InterruptBridge.dispatch._1_1JJJJJ");

void kernel_entry(BootInfo* info) {
    // BootInfo 在 bootloader 栈上（物理地址）；内核已运行于高半区，
    // 经 physmap 映射访问（trampoline 的引导表仍含 identity，但规范表不含）。
    BootInfo* vi = (BootInfo*)(uintptr_t)phys_to_virt((uintptr_t)info);
    // 无 GOP 时 framebuffer 为 0，width/height/stride 不赋值（保持 0）。
    if (vi->framebuffer) {
        gop_framebuffer = vi->framebuffer;
        gop_width  = vi->width;
        gop_height = vi->height;
        gop_stride = vi->stride;
    }
    uefi_mmap_addr = vi->memoryMap;
    uefi_mmap_size = vi->memoryMapSize;
    uefi_mmap_desc_size = vi->memoryMapDescriptorSize;

    asm volatile("cli");
    // 自己的扁平 GDT（kernel CS 0x08），再按它重载 IDT
    init_gdt();
    init_syscall();
    init_idt(0x08);
    // sti deferred to Partic after all init is done

    extern void _start(void);
    _start();

    while (1) __asm__("hlt");
}
