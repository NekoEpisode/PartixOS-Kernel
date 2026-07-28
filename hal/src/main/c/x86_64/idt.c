#include "include/idt.h"
#include "include/stdint.h"

typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_middle;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_ptr_t;

static idt_entry_t idt[256] __attribute__((aligned(16)));

extern void* isr_handlers[256];

void init_idt(uint16_t uefi_cs_selector) {
    for (int i = 0; i < 256; i++) {
        uint64_t addr = (uint64_t)isr_handlers[i];
        idt_entry_t* entry = &idt[i];
        entry->offset_low    = addr & 0xFFFF;
        entry->selector      = uefi_cs_selector;
        entry->ist           = 0;
        entry->type_attr     = 0x8E;
        entry->offset_middle = (addr >> 16) & 0xFFFF;
        entry->offset_high   = (addr >> 32) & 0xFFFFFFFF;
        entry->reserved      = 0;
    }

    idt_ptr_t idtr = {
        .limit = sizeof(idt) - 1,
        .base  = (uint64_t)idt
    };
    asm volatile("lidt %0" : : "m"(idtr));
}