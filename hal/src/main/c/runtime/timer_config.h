#ifndef TIMER_CONFIG_H
#define TIMER_CONFIG_H

// Single source of truth for the tick rate: kr.partix.kernel.timer.Timer.TICK_HZ
// (Partic side). C code references the same constant through its mangled
// symbol, so changing the frequency only requires editing Timer.partic.
// mangled: P_kr_partix_kernel_timer_Timer.s.TICK_1HZ (含 '.'，非合法 C
// 标识符，用 __asm__ 绑定汇编符号名)
extern const unsigned long kr_timer_tick_hz
    __asm__("P_kr_partix_kernel_timer_Timer.s.TICK_1HZ");

#define TIMER_TICK_HZ (kr_timer_tick_hz)
#define TIMER_TICK_MS (1000 / TIMER_TICK_HZ)   // milliseconds per tick

#endif
