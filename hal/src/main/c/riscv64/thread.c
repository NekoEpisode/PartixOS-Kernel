// RISC-V 64 thread support: initial-frame construction + per-tid saved-frame
// slots. The slot array is also referenced by context.S (switch_frame).

#include <stdint.h>

#define FRAME_SLOTS 34   // 30 GP regs + sepc + sstatus + saved_sp + kstack_top

// Saved-frame slot per tid (slot 0 is a scratch slot used by Scheduler.start).
uint64_t thread_frames[256];

long thread_frame_get(long tid) {
    return (long)thread_frames[tid];
}

void thread_frame_set(long tid, long frame) {
    thread_frames[tid] = (uint64_t)frame;
}

// x86-only hook; no-op on RISC-V (sscratch carries the kernel stack top).
void g_current_kstack_top_set(long top) {
    (void)top;
}

// Build a 272-byte initial trap frame for a new thread.
// Slot layout: 0..29 = ra..t6, 30 = sepc, 31 = sstatus, 32 = saved_sp,
// 33 = kernel stack top.
void build_thread_frame(uint64_t *frame, uint64_t entry, uint64_t arg,
                        uint64_t saved_sp, uint64_t kstack_top,
                        uint64_t sstatus) {
    for (int i = 0; i < FRAME_SLOTS; i++) frame[i] = 0;
    frame[8] = arg;        // a0
    frame[30] = entry;     // sepc
    frame[31] = sstatus;
    frame[32] = saved_sp;
    frame[33] = kstack_top;
}

// Thread entry trampoline: hands the runnable to the fixed Partic entry
// (kr.partix.kernel.sched.KernelThreads.run) and never returns.
extern void kr_partix_kernel_sched_KernelThreads_run__JV(long arg);

void kthread_entry_stub(long arg) {
    kr_partix_kernel_sched_KernelThreads_run__JV(arg);
    for (;;) __asm__ volatile("wfi");
}

void idle_entry_stub(long arg) {
    (void)arg;
    for (;;) __asm__ volatile("wfi");
}

long kthread_entry_stub_addr = (long)(void *)kthread_entry_stub;
long idle_entry_stub_addr = (long)(void *)idle_entry_stub;
