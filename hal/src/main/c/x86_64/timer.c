// x86_64 timer: Local APIC timer (primary) with PIT auto-fallback.
//
// The APIC timer is a per-CPU, high-precision timer driven by the CPU's bus
// clock. We calibrate its counting rate against PIT channel 2 (1.193182 MHz,
// guaranteed to exist on any PC) so the code works on QEMU and real hardware
// alike. If the Local APIC cannot be enabled or calibration yields an absurd
// result, we fall back to PIT channel 0 (IRQ0, ~100 Hz) — the classic x86
// timer that always works.
//
// Exposed uniform API (also implemented by riscv64/timer.c):
//   int  timer_init()   - program the timer; 0 on success, negative on error
//   void timer_tick()   - one timer interrupt: bump g_tick (called from dispatch)
//   void timer_eoi()    - end-of-interrupt for the active backend
//   long timer_ticks()  - current tick counter (g_tick)
//   int  timer_vector() - IDT vector the timer delivers on (-1 if none)
//
// g_tick increments at 100 Hz; sys_now() already derives ms as g_tick * 10.

#include "include/stdint.h"

extern volatile unsigned long g_tick;

// ── MSR ────────────────────────────────────────────────
#define APIC_BASE_MSR        0x1B
#define APIC_BASE_EN         (1ULL << 11)
#define APIC_BASE_X2APIC     (1ULL << 10)
#define APIC_BASE_ADDR_MASK  0xFFFFF000ULL

// ── Local APIC registers (offsets from base) ───────────
#define LAPIC_SVR            0xF0
#define LAPIC_TPR            0x80
#define LAPIC_LVT_TIMER      0x320
#define LAPIC_TIMER_INITCNT  0x380
#define LAPIC_TIMER_CURCNT   0x390
#define LAPIC_TIMER_DIV      0x3E0
#define LAPIC_EOI            0xB0

#define LAPIC_TIMER_VECTOR   0x30   // above PIC IRQ range (0x20-0x2F)
#define PIT_TIMER_VECTOR     0x20   // IRQ0 via PIC

#define LAPIC_LVT_MASK       (1u << 16)
#define LAPIC_LVT_PERIODIC   (1u << 17)
#define LAPIC_DIV16          0x3    // 0b0011 -> /16

// ── PIT (8254) ─────────────────────────────────────────
#define PIT_CMD              0x43
#define PIT_CH0              0x40
#define PIT_CH2              0x42
#define PIT_SPKR             0x61
#define PIT_SPKR_GATE2       0x01
#define PIT_SPKR_OUT2        0x20
#define PIT_FREQ             1193182UL
#define PIT_CH2_ONESHOT      0xB0   // ch2, mode 0, lo/hi, binary
#define PIT_CH0_RATE         0x34   // ch0, mode 2, lo/hi, binary

static volatile uint32_t *lapic = 0;
static int active_vector = -1;
static int using_pit = 0;

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr) : "memory");
    return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr(uint32_t msr, uint64_t v) {
    uint32_t lo = (uint32_t)v, hi = (uint32_t)(v >> 32);
    __asm__ volatile("wrmsr" : : "a"(lo), "d"(hi), "c"(msr) : "memory");
}

static inline void outb(uint16_t port, uint8_t v) {
    __asm__ volatile("outb %0, %1" :: "a"(v), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline uint32_t lapic_read(uint32_t reg) {
    return lapic[reg >> 2];
}

static inline void lapic_write(uint32_t reg, uint32_t v) {
    lapic[reg >> 2] = v;
}

// ── Local APIC discovery / enable ──────────────────────
static int apic_probe(void) {
    uint64_t v = rdmsr(APIC_BASE_MSR);
    uint64_t base = v & APIC_BASE_ADDR_MASK;
    if (!base) return -1;

    if (v & APIC_BASE_X2APIC) {
        // x2APIC is active: switch back to xAPIC (MMIO) mode.
        // Must clear EN first, then set the new mode + EN (interrupts are off).
        wrmsr(APIC_BASE_MSR, v & ~APIC_BASE_EN);
        v = rdmsr(APIC_BASE_MSR);
        wrmsr(APIC_BASE_MSR, (v & ~APIC_BASE_X2APIC) | APIC_BASE_EN);
    } else if (!(v & APIC_BASE_EN)) {
        wrmsr(APIC_BASE_MSR, v | APIC_BASE_EN);
    }

    lapic = (volatile uint32_t *)(unsigned long)base;
    return 0;
}

// ── LAPIC timer setup (masked) ─────────────────────────
static void lapic_timer_setup(void) {
    lapic_write(LAPIC_SVR, lapic_read(LAPIC_SVR) | 0x100 | 0xFF); // enable APIC
    lapic_write(LAPIC_TPR, 0);
    lapic_write(LAPIC_LVT_TIMER, LAPIC_LVT_MASK);                 // masked
    lapic_write(LAPIC_TIMER_DIV, LAPIC_DIV16);
}

// Returns counts (at /16) per 10 ms, or 0 if calibration failed.
static uint32_t lapic_calibrate_per10ms(void) {
    // Arm the LAPIC timer one-shot with the max count.
    lapic_write(LAPIC_LVT_TIMER, LAPIC_LVT_MASK | LAPIC_TIMER_VECTOR);
    lapic_write(LAPIC_TIMER_INITCNT, 0xFFFFFFFFu);

    // PIT channel 2, mode 0, count 0xFFFF -> ~54.93 ms window.
    outb(PIT_SPKR, inb(PIT_SPKR) | PIT_SPKR_GATE2);
    outb(PIT_CMD, PIT_CH2_ONESHOT);
    outb(PIT_CH2, 0xFF);
    outb(PIT_CH2, 0xFF);

    uint64_t spins = 0;
    while (!(inb(PIT_SPKR) & PIT_SPKR_OUT2)) {
        if (++spins > 0x40000000ULL) {   // PIT never fired — give up
            lapic_write(LAPIC_TIMER_INITCNT, 0);
            return 0;
        }
    }

    uint32_t cur = lapic_read(LAPIC_TIMER_CURCNT);
    lapic_write(LAPIC_TIMER_INITCNT, 0);

    uint32_t elapsed = 0xFFFFFFFFu - cur; // counts over 54,933.3 us
    uint64_t per10ms = ((uint64_t)elapsed * 10000) / 54933;
    if (per10ms == 0 || per10ms > 0xFFFFFF00ULL) return 0;
    return (uint32_t)per10ms;
}

static void lapic_timer_start(uint32_t per10ms) {
    lapic_write(LAPIC_LVT_TIMER, LAPIC_TIMER_VECTOR | LAPIC_LVT_PERIODIC);
    lapic_write(LAPIC_TIMER_INITCNT, per10ms);
    active_vector = LAPIC_TIMER_VECTOR;
    using_pit = 0;
}

// ── PIT fallback (~100 Hz, IRQ0) ───────────────────────
static void pit_timer_start(void) {
    outb(PIT_CMD, PIT_CH0_RATE);
    uint16_t cnt = (uint16_t)(PIT_FREQ / 100);   // 11932 -> 100.0 Hz
    outb(PIT_CH0, cnt & 0xFF);
    outb(PIT_CH0, cnt >> 8);
    // Unmask IRQ0 on the master PIC so the PIT interrupt can reach the CPU.
    outb(0x21, inb(0x21) & ~0x01u);
    active_vector = PIT_TIMER_VECTOR;
    using_pit = 1;
}

// ── Public API ─────────────────────────────────────────
int timer_init(void) {
    if (apic_probe() != 0) {
        pit_timer_start();
        return 0;
    }

    lapic_timer_setup();
    uint32_t per10ms = lapic_calibrate_per10ms();
    if (!per10ms) {
        pit_timer_start();
        return 0;
    }

    // LAPIC timer active: silence PIT channel 0 (IRQ0 masked by the PIC init,
    // enforce it here so a stray PIT can never deliver a second tick stream).
    outb(0x21, inb(0x21) | 0x01u);
    lapic_timer_start(per10ms);
    return 0;
}

void timer_tick(void) {
    g_tick++;
}

void timer_eoi(void) {
    if (using_pit) {
        outb(0x20, 0x20);                    // PIC1 EOI
    } else if (lapic) {
        lapic_write(LAPIC_EOI, 0);           // LAPIC EOI
    }
}

long timer_ticks(void) {
    return (long)g_tick;
}

int timer_vector(void) {
    return active_vector;
}

// RISC-V-only helpers; no-ops on x86 (all arch Partic classes link together).
long timer_sbi_error(void) {
    return 0;
}

void timer_clear_ssip(void) {
}
