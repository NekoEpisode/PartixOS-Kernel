#include "include/idt.h"
#include "include/stdint.h"
#include "include/io.h"

typedef struct {
    uint16_t offset_low;
    uint16_t selector;      // 代码段选择子
    uint8_t  ist;           // 中断栈表，通常0
    uint8_t  type_attr;     // P|DPL|门类型
    uint16_t offset_middle;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_ptr_t;

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t vector, error_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) interrupt_frame_t;

static idt_entry_t idt[256] __attribute__((aligned(16)));

static uint16_t uefi_cs_selector = 0;

static void idt_zero_memory(void* dest, unsigned long size) {
    unsigned char* p = (unsigned char*)dest;
    for (unsigned long i = 0; i < size; i++) {
        p[i] = 0;
    }
}

static void idt_set_gate(idt_entry_t* entry, void* handler, uint16_t selector, uint8_t type_attr) {
    uint64_t addr = (uint64_t)handler;
    entry->offset_low    = addr & 0xFFFF;
    entry->selector      = selector;
    entry->ist           = 0;
    entry->type_attr     = type_attr;
    entry->offset_middle = (addr >> 16) & 0xFFFF;
    entry->offset_high   = (addr >> 32) & 0xFFFFFFFF;
    entry->reserved      = 0;
}

// Bridges interrupt to Partic
extern void kr_partix_kernel_interrupt_InterruptBridge_dispatch__IJJJV(
    int cause, uint64_t epc, uint64_t sp, uint64_t frame);

__attribute__((interrupt))
void default_handler(interrupt_frame_t* frame) {
    kr_partix_kernel_interrupt_InterruptBridge_dispatch__IJJJV(
        (int)(frame->vector), frame->rip, frame->rsp, (uint64_t)frame);
}

void init_idt(uint16_t uefi_cs_selector) {
    idt_zero_memory(idt, sizeof(idt));

    for (int i = 0; i < 256; i++) {
        idt_set_gate(&idt[i], default_handler, uefi_cs_selector, 0x8E);
    }

    idt_ptr_t idtr = {
        .limit = sizeof(idt) - 1,
        .base  = (uint64_t)idt
    };
    asm volatile("lidt %0" : : "m"(idtr));
    asm volatile("sti");
}