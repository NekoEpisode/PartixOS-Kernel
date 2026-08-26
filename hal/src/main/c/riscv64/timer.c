// RISC-V 64 timer (S-mode, over OpenSBI).
//
// The kernel runs in S-mode under OpenSBI, so the CLINT/MTIMER mtimecmp
// registers (0x02000000 area) are M-mode-only: writing them from S-mode is
// blocked by the PMP. The sanctioned way to drive the CLINT timer from
// S-mode is the SBI TIME extension (ecall), which OpenSBI turns into an
// mtimecmp write on our behalf. Reading the time uses `rdtime` (the `time`
// CSR is readable from S-mode). This is exactly what Linux/Xen do on the
// same platform, so it is not coupled to QEMU and works on real hardware
// with any SBI-compliant M-mode firmware (OpenSBI, etc.).
//
// One-shot semantics: each tick re-arms the next deadline
// (mtime + timebase/TIMER_TICK_HZ) so we run at TIMER_TICK_HZ.
//
// Exposed uniform API (also implemented by x86_64/timer.c):
//   int  timer_init()          - program the timer; 0 on success, negative on error
//   void timer_tick()          - one timer interrupt: bump g_tick + re-arm
//   void timer_eoi()           - no-op for the RISC-V timer (no claim/complete)
//   long timer_ticks()         - current tick counter (g_tick)
//   int  timer_vector()        - unused on RISC-V, returns -1
//   long timer_sbi_error()     - last SBI set_timer error (0 = success)
//   void timer_clear_ssip()    - clear supervisor software interrupt pending

#include <stdint.h>
#include "../runtime/timer_config.h"

extern volatile unsigned long g_tick;

#define SIE_STIE_BIT        5
#define TIMEBASE_FALLBACK   10000000ULL   // QEMU virt & most boards: 10 MHz

#define SBI_SUCCESS         0

extern uint64_t fdt_get_timebase_freq(void);

// Implemented in asm/riscv64/sbi.S (hand-written so the ecall argument
// registers a6/a7 are guaranteed; inline asm register binding was unreliable).
extern long sbi_set_timer(uint64_t stime_value);
extern long sbi_set_timer_legacy(uint64_t stime_value);

static uint64_t timebase = TIMEBASE_FALLBACK;
static long last_sbi_error = 0;

static inline uint64_t read_time(void) {
    uint64_t t;
    __asm__ volatile("rdtime %0" : "=r"(t));
    return t;
}

static void set_next_deadline(void) {
    uint64_t next = read_time() + timebase / TIMER_TICK_HZ;

    // Modern SBI v0.2+ TIME extension first, legacy SBI v0.1 fallback.
    last_sbi_error = sbi_set_timer(next);
    if (last_sbi_error != SBI_SUCCESS) {
        last_sbi_error = sbi_set_timer_legacy(next);
    }
}

int timer_init(void) {
    // Sanity-clamp the FDT timebase: a corrupt/absent value must not
    // overflow the deadline computation (which would storm STIP).
    uint64_t tb = fdt_get_timebase_freq();
    if (tb >= 100000ULL && tb <= 1000000000ULL) {
        timebase = tb;
    }

    // Enable the supervisor timer interrupt (STIE) in sie.
    // (io.c's enableInterrupts() also writes sie with SSIE|STIE|SEIE, so this
    // is defensive; the first deadline must be armed before interrupts open.)
    // Note: STIE is bit 5, which does not fit the 5-bit immediate of csrsi.
    uint64_t stie_bit = 1u << SIE_STIE_BIT;
    __asm__ volatile("csrrs zero, sie, %0" :: "r"(stie_bit));

    set_next_deadline();
    return last_sbi_error == SBI_SUCCESS ? 0 : -1;
}

void timer_tick(void) {
    g_tick++;
    set_next_deadline();
}

void timer_eoi(void) {
    // The RISC-V timer has no EOI: clearing the pending bit happens
    // implicitly when mtimecmp is re-armed past mtime.
}

long timer_ticks(void) {
    return (long)g_tick;
}

int timer_vector(void) {
    return -1;   // RISC-V dispatches on scause, not on an IDT vector
}

long timer_sbi_error(void) {
    return last_sbi_error;
}

void timer_clear_ssip(void) {
    // Clear the supervisor software interrupt pending bit.
    // NOTE: per the RISC-V priv spec (v1.12+) SSIP is a plain RW bit: write 1
    // SETS the pending flag, write 0 CLEARS it. Writing 1 (W1C-style, as the
    // old v1.10 spec did) actually re-arms the interrupt — do NOT do that.
    __asm__ volatile("csrwi sip, 0");
}
