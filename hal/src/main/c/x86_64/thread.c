// x86_64 thread support: per-tid saved-frame slots, initial-frame
// construction, thread entry trampolines, current kernel stack top.

#include <stdint.h>

// Saved-frame slot per tid (slot 0 is a scratch slot used by Scheduler.start).
uint64_t thread_frames[256];

// Kernel stack top of the current thread. Written on every thread switch;
// the ISR and syscall entry use it to build frames / switch stacks.
volatile uint64_t g_current_kstack_top;

long thread_frame_get(long tid) {
    return (long)thread_frames[tid];
}

void thread_frame_set(long tid, long frame) {
    thread_frames[tid] = (uint64_t)frame;
}

// Size in bytes of the saved context frame for this architecture.
long thread_frame_size() {
    return 184;
}

void g_current_kstack_top_set(long top) {
    g_current_kstack_top = (uint64_t)top;
}

// Build a 184-byte initial frame for a new thread.
// Slot layout (8-byte units): 0..14 = r15..rax, 15 = vector, 16 = errcode,
// 17 = RIP, 18 = CS, 19 = RFLAGS, 20 = RSP, 21 = SS, 22 = kernel stack top.
// sstatus encodes the mode: SPP bit (0x100) set = kernel thread (kept from
// the riscv convention).
// Kernel threads use the current CS/SS (boots on the UEFI GDT before ours);
// user threads use our hardcoded ring3 selectors (valid once the GDT is up).
void build_thread_frame(uint64_t *frame, uint64_t entry, uint64_t arg,
                        uint64_t saved_sp, uint64_t kstack_top,
                        uint64_t sstatus) {
    int user = (sstatus & 0x100) == 0;
    uint32_t cs = 0, ss = 0;
    __asm__ volatile("mov %%cs, %0" : "=r"(cs));
    __asm__ volatile("mov %%ss, %0" : "=r"(ss));
    for (int i = 0; i < 23; i++) frame[i] = 0;
    frame[9]  = arg;                      // rdi slot: kernel stub arg
    frame[15] = 0xFE;                     // fake vector
    frame[17] = entry;                    // RIP
    frame[18] = user ? 0x1B : (cs & 0xFFFF);   // CS (ring3 / current)
    frame[19] = 0x202;                    // RFLAGS: IF
    frame[20] = saved_sp;                 // RSP
    frame[21] = user ? 0x23 : (ss & 0xFFFF);   // SS (ring3 / current)
    frame[22] = kstack_top;               // kernel stack top
}

// Thread entry trampoline: hands the runnable to the fixed Partic entry
// (kr.partix.kernel.sched.KernelThreads.run) and never returns.
extern void kr_partix_kernel_sched_KernelThreads_run__JV(long arg);

void kthread_entry_stub(long arg) {
    kr_partix_kernel_sched_KernelThreads_run__JV(arg);
    for (;;) __asm__ volatile("cli; hlt");
}

void idle_entry_stub(long arg) {
    (void)arg;
    // 空闲线程必须保持中断开启（不能 cli），否则 timer 无法唤醒睡眠线程
    for (;;) __asm__ volatile("hlt");
}

long kthread_entry_stub_addr = (long)(void *)kthread_entry_stub;
long idle_entry_stub_addr = (long)(void *)idle_entry_stub;
