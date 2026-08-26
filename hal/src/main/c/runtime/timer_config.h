#ifndef TIMER_CONFIG_H
#define TIMER_CONFIG_H

// Single source of truth for the tick rate: kr.partix.kernel.timer.Timer.TICK_HZ
// (Partic side). C code references the same constant through its mangled
// symbol, so changing the frequency only requires editing Timer.partic.
extern const unsigned long kr_partix_kernel_timer_Timer_s_TICK_HZ;

#define TIMER_TICK_HZ (kr_partix_kernel_timer_Timer_s_TICK_HZ)
#define TIMER_TICK_MS (1000 / TIMER_TICK_HZ)   // milliseconds per tick

#endif
