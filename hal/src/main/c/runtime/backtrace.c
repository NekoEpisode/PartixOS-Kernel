// Kernel backtrace helpers on top of LLVM libunwind (static, bare-metal).
//   kr_backtrace_current    — unwind the current call chain.
//   kr_backtrace_from_trap  — unwind the interrupted context from a RISC-V
//                              trap frame (entry.S layout), so the faulting
//                              code's callers are visible.
// All frames come back as raw PC values; symbol/line decoration is a
// separate concern (no debug tables in memory).

#include <stdint.h>

// ---- minimal LLVM libunwind C API declarations (no headers in tree) ----
typedef int _Unwind_Reason_Code;
struct _Unwind_Context;
typedef _Unwind_Reason_Code (*_Unwind_Trace_Fn)(struct _Unwind_Context *, void *);
extern _Unwind_Reason_Code _Unwind_Backtrace(_Unwind_Trace_Fn, void *);
extern uintptr_t _Unwind_GetIP(struct _Unwind_Context *);

typedef uintptr_t unw_word_t;
typedef int unw_regnum_t;
// riscv64 sizes from LLVM libunwind __libunwind_config.h:
//   context = 32*(xlen+flen)/64 words = 64 words (regs + fp regs)
//   cursor  = context + 7..12 words depending on flen; give headroom.
typedef struct { uint64_t data[64]; } unw_context_t;
typedef struct { uint64_t data[80]; } unw_cursor_t;
extern int unw_init_local(unw_cursor_t *, unw_context_t *);
extern int unw_step(unw_cursor_t *);
extern int unw_get_reg(unw_cursor_t *, unw_regnum_t, unw_word_t *);
#define UNW_REG_IP (-1)

#define BT_MAX_FRAMES 32

static uint64_t *bt_pcs;
static int bt_max;
static int bt_count;

static _Unwind_Reason_Code bt_callback(struct _Unwind_Context *ctx, void *arg) {
    (void)arg;
    // Frame 0 is _Unwind_Backtrace / kr_backtrace_current itself — drop it.
    if (bt_count > 0 && bt_count - 1 < bt_max)
        bt_pcs[bt_count - 1] = (uint64_t)_Unwind_GetIP(ctx);
    bt_count++;
    return 0;  // _URC_NO_REASON
}

int kr_backtrace_current(uint64_t *pcs, int max) {
    if (!pcs || max <= 0) return 0;
    if (max > BT_MAX_FRAMES) max = BT_MAX_FRAMES;
    bt_pcs = pcs;
    bt_max = max;
    bt_count = 0;
    _Unwind_Backtrace(bt_callback, 0);
    return bt_count < max ? bt_count : max;
}

#ifdef __riscv
// entry.S trap frame (8-byte slots):
//   [0]=ra [1]=gp [2]=tp [3]=t0 [4]=t1 [5]=t2 [6]=s0 [7]=s1
//   [8..15]=a0..a7 [16..25]=s2..s11 [26]=t3 [27]=t4 [28]=t5 [29]=t6
//   [30]=sepc [31]=sstatus
int kr_backtrace_from_trap(uint64_t *pcs, int max, uint64_t epc, uint64_t frame) {
    if (!pcs || max <= 0 || !frame) return 0;
    if (max > BT_MAX_FRAMES) max = BT_MAX_FRAMES;

    const uint64_t *f = (const uint64_t *)frame;
    unw_context_t ctx;
    uint64_t *r = (uint64_t *)&ctx;
    uint64_t int_sp;
    __asm__ volatile("csrr %0, sscratch" : "=r"(int_sp));  // interrupted sp

    r[0] = epc;            // pc
    r[1] = f[0];           // ra
    r[2] = int_sp;         // sp
    r[3] = f[1];           // gp
    r[4] = f[2];           // tp
    r[5] = f[3];           // t0
    r[6] = f[4];           // t1
    r[7] = f[5];           // t2
    r[8] = f[6];           // s0
    r[9] = f[7];           // s1
    for (int i = 0; i < 8; i++)  r[10 + i] = f[8 + i];    // a0..a7
    for (int i = 0; i < 10; i++) r[18 + i] = f[16 + i];   // s2..s11
    r[28] = f[26];         // t3
    r[29] = f[27];         // t4
    r[30] = f[28];         // t5
    r[31] = f[29];         // t6
    // float registers are not needed for unwinding

    unw_cursor_t cursor;
    if (unw_init_local(&cursor, &ctx) != 0) return 0;

    int n = 0;
    do {
        if (n < max) {
            unw_word_t ip = 0;
            unw_get_reg(&cursor, UNW_REG_IP, &ip);
            pcs[n] = (uint64_t)ip;
        }
        n++;
    } while (unw_step(&cursor) > 0 && n <= max);
    return n < max ? n : max;
}
#else
int kr_backtrace_from_trap(uint64_t *pcs, int max, uint64_t epc, uint64_t frame) {
    (void)pcs; (void)max; (void)epc; (void)frame;
    return 0;  // x86 trap-frame unwinding not implemented yet
}
#endif
