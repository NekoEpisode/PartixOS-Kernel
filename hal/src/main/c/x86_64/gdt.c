// x86_64 GDT + TSS.
//
// Own GDT (replaces the UEFI one):
//   0x00 null
//   0x08 kernel code (ring0, 64-bit)
//   0x10 kernel data (ring0)
//   0x18 user code   (ring3, 64-bit)
//   0x20 user data   (ring3)
//   0x28 TSS         (64-bit, holds RSP0 for ring3 -> ring0 traps)
//
// Selector layout keeps STAR (EFER) sycall layout simple: kernel CS | 0x10
// = user CS (0x08 -> 0x18), so STAR.SYSCALL_CS = 0x08 | (0x18 << 48) later.
//
// TSS.RSP0 is updated by restore_frame on every context switch so a ring3
// interrupt lands on the current thread's kernel stack.

#include "include/stdint.h"

#define GDT_ENTRIES 7

#define GDT_KCODE 0x08
#define GDT_KDATA 0x10
#define GDT_UCODE 0x18
#define GDT_UDATA 0x20
#define GDT_TSS   0x28

// 64-bit TSS: 104 bytes. Only RSP0 (offset 4..11) is used for now.
typedef struct {
    uint32_t rsvd0;      // +0
    uint64_t rsp0;       // +4  (low) +8 (high)
    uint64_t rsp1;       // +12
    uint64_t rsp2;       // +20
    uint64_t rsvd1;      // +28
    uint64_t ist[7];     // +36..+91
    uint64_t rsvd2;      // +92
    uint16_t rsvd3;      // +100
    uint16_t iopb;       // +102  (0xFFFF = no IO bitmap)
} __attribute__((packed)) tss64_t;

static tss64_t tss __attribute__((aligned(16)));
static uint64_t gdt[GDT_ENTRIES] __attribute__((aligned(16)));

// GDT entry: base/limit for flat segments; access byte + flags byte.
static void gdt_set(int idx, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags) {
    gdt[idx] = (uint64_t)(limit & 0xFFFF)
             | ((uint64_t)(base & 0xFFFFFF) << 16)
             | ((uint64_t)access << 40)
             | ((uint64_t)((limit >> 16) & 0x0F) << 48)
             | ((uint64_t)(flags & 0x0F) << 52)
             | ((uint64_t)((base >> 24) & 0xFF) << 56);
}

// TSS descriptor spans two slots (idx, idx+1).
static void tss_set_descriptor(int idx) {
    uint64_t base = (uint64_t)&tss;
    uint32_t limit = sizeof(tss64_t) - 1;

    uint64_t lo = (uint64_t)(limit & 0xFFFF)
                | ((base & 0xFFFFFF) << 16)
                | (0x89ULL << 40)              // present, 64-bit TSS, available
                | ((uint64_t)((limit >> 16) & 0x0F) << 48)
                | ((uint64_t)((base >> 24) & 0xFF) << 56);
    gdt[idx]     = lo;
    gdt[idx + 1] = (base >> 32) & 0xFFFFFFFF;  // high base; limit is zero-extended
}

// Reload all segment registers with our selectors.
// CS is switched by a far return so 64-bit mode is preserved.
static void reload_segments(void) {
    // far return: push new CS + RIP of next instruction
    __asm__ volatile(
        "leaq 1f(%%rip), %%rax\n\t"
        "pushq %0\n\t"
        "pushq %%rax\n\t"
        "lretq\n"
        "1:\n\t"
        : : "i"(GDT_KCODE) : "rax", "memory");

    __asm__ volatile(
        "mov %0, %%ds\n\t"
        "mov %0, %%es\n\t"
        "mov %0, %%fs\n\t"
        "mov %0, %%gs\n\t"
        "mov %0, %%ss\n\t"
        : : "r"((uint16_t)GDT_KDATA) : "memory");

    // load TSS; low 16 bits of the selector are the TSS index
    __asm__ volatile("ltr %0" : : "r"((uint16_t)GDT_TSS) : "memory");
}

// Install the flat GDT + TSS and switch all segments over.
void init_gdt(void) {
    for (int i = 0; i < GDT_ENTRIES; i++) gdt[i] = 0;

    gdt_set(GDT_KCODE / 8, 0, 0xFFFFF, 0x9A, 0xA);  // ring0 code, 64-bit
    gdt_set(GDT_KDATA / 8, 0, 0xFFFFF, 0x92, 0xC);  // ring0 data, 32-bit defaults
    gdt_set(GDT_UCODE / 8, 0, 0xFFFFF, 0xFA, 0xA);  // ring3 code, 64-bit
    gdt_set(GDT_UDATA / 8, 0, 0xFFFFF, 0xF2, 0xC);  // ring3 data
    tss_set_descriptor(GDT_TSS / 8);

    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) gdtr;
    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base  = (uint64_t)gdt;

    __asm__ volatile("lgdt %0" : : "m"(gdtr) : "memory");
    reload_segments();
}

// Called from restore_frame (asm) on every context switch.
// frame+176 holds the next thread's kernel stack top. Before the scheduler
// starts that slot is 0 (g_current_kstack_top unset) — skip then, since no
// thread stack switch is needed (and frame+176 would be an invalid read).
void gdt_set_rsp0_from_frame(uint64_t frame) {
    if (frame == 0) return;
    uint64_t kstack_top = *(uint64_t *)(frame + 176);
    if (kstack_top != 0) {
        tss.rsp0 = kstack_top;
    }
}

// Current thread's kernel stack top, for syscall_entry (ring3 -> ring0).
uint64_t gdt_get_rsp0(void) {
    return tss.rsp0;
}

// ── syscall instruction setup (EFER.SCE / STAR / LSTAR / SFMASK) ──
// The syscall entry builds an ISR-style frame and returns via iretq, so
// only STAR's SYSCALL half (kernel CS) matters; the SYSRET half is unused.
#define MSR_EFER   0xC0000080
#define MSR_STAR   0xC0000081
#define MSR_LSTAR  0xC0000082
#define MSR_SFMASK 0xC0000084
#define EFER_SCE   0x001
#define EFER_NXE   0x800

extern void syscall_entry(void);

static inline uint64_t rdmsr64(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr64(uint32_t msr, uint64_t v) {
    uint32_t lo = (uint32_t)v, hi = (uint32_t)(v >> 32);
    __asm__ volatile("wrmsr" : : "a"(lo), "d"(hi), "c"(msr) : "memory");
}

// Enable the syscall/sysret instruction and NX (the page tables use NX
// bits; without EFER.NXE any NX-marked PTE faults with #GP).
void init_syscall(void) {
    // NX must be enabled before any NX-marked page table is loaded.
    uint64_t efer = rdmsr64(MSR_EFER);
    wrmsr64(MSR_EFER, efer | EFER_SCE | EFER_NXE);

    // STAR: SYSCALL CS = 0x08 (bits 47:32); SS = CS+8 = 0x10.
    wrmsr64(MSR_STAR, (uint64_t)GDT_KCODE << 32);
    wrmsr64(MSR_LSTAR, (uint64_t)(unsigned long)syscall_entry);
    // SFMASK: clear IF (bit 9) on syscall entry (interrupts stay off in
    // the handler; DF is left alone).
    wrmsr64(MSR_SFMASK, 0x200);
}
