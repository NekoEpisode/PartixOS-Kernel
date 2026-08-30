# x86_64 syscall entry (LSTAR target).
#
# The syscall instruction does NOT go through the IDT and does NOT switch
# stacks: it jumps to LSTAR with CS/SS switched to ring0 (per STAR MSR),
# saves user RIP in RCX and user RFLAGS in R11, and leaves user RSP intact.
# So we must:
#   1. save user RSP, switch to the current thread's kernel stack
#      (g_current_kstack_top, kept in sync by the scheduler),
#   2. build the full ISR-style 7-slot frame (vector = 0x80 marks syscall),
#   3. run the shared dispatch path (InterruptBridge → arch dispatch).
# Return goes through restore_frame's iretq (not sysretq), which pops the
# user CS/SS/RSP/RFLAGS we stored explicitly — no STAR SYSRET segment
# gymnastics needed.
#
# ABI (Linux-compatible): eax = number, rdi/rsi/rdx/r10/r8/r9 = args.
# Frame slots: +112 rax(num), +72 rdi, +80 rsi, +88 rdx, +40 r10,
#              +48 r8, +56 r9. RCX/R11 are clobbered by syscall (user RIP /
# user RFLAGS), matching Linux semantics.

.section .text

# user RSP is stashed here until the frame is built (single-threaded use:
# consumed within syscall_entry; per-thread stacks come from the scheduler).
.section .bss
.align 8
saved_user_rsp:
    .quad 0
.section .text

.globl syscall_entry
syscall_entry:
    movq %rsp, saved_user_rsp(%rip)      # save user RSP before switching
    movq g_current_kstack_top(%rip), %rsp # current thread's kernel stack
    # Reserve the (future) red zone below the frame, like isr_common.
    subq $128, %rsp

    # ── 7 slots (SS first → +168 ... vector last → +120) ──
    pushq $0x23                           # SS  (+168) user data, RPL3
    pushq saved_user_rsp(%rip)            # RSP (+160) user stack
    pushq %r11                            # RFLAGS (+152) user RFLAGS
    pushq $0x1B                           # CS  (+144) user code, RPL3
    pushq %rcx                            # RIP (+136) user RIP (next insn)
    pushq $0                              # errcode (+128)
    pushq $0x80                           # vector (+120) syscall marker

    # ── 15 GP regs (rax first → +112 ... r15 last → +0) ──
    pushq %rax                            # +112 syscall number (user eax)
    pushq %rbx                            # +104
    pushq %rcx                            # +96  (user RIP — Linux clobbers rcx)
    pushq %rdx                            # +88  arg2
    pushq %rsi                            # +80  arg1
    pushq %rdi                            # +72  arg0
    pushq %rbp                            # +64
    pushq %r8                             # +56  arg5
    pushq %r9                             # +48  arg4
    pushq %r10                            # +40  arg3
    pushq %r11                            # +32  (user RFLAGS — Linux clobbers r11)
    pushq %r12                            # +24
    pushq %r13                            # +16
    pushq %r14                            # +8
    pushq %r15                            # +0

    # kstack_top slot at +176
    movq g_current_kstack_top(%rip), %rax
    movq %rax, 176(%rsp)

    cld

    # rdi = cause (0x80), rsi = epc (user RIP), rdx = sp, rcx = frame
    movq %rsp, %rcx
    movq $0x80, %rdi
    movq 136(%rsp), %rsi
    movq %rsp, %rdx

    call P_kr_partix_kernel_interrupt_InterruptBridge.dispatch._1_1JJJJJ

    # Restore from the returned frame (same frame if no context switch)
    movq %rax, %rsp
    jmp  restore_frame
