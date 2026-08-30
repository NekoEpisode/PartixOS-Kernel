# RISC-V 64 kernel entry (UEFI S-mode)

.section .text.entry
.globl kern_entry
.align 4

kern_entry:
    mv  s0, a0

    la  sp, _stack_top
    la  gp, __global_pointer$

    # Disable all interrupts inherited from UEFI
    csrw sie, zero
    csrci sstatus, 0x2

    # Enable FPU: mstatus.FS = 01 (Initial). The kernel is built hard-float
    # (-mabi=lp64d) and libunwind's unw_getcontext saves F registers; with
    # FS=0 the first FP instruction (e.g. unw_getcontext's fsd) traps as an
    # illegal instruction, which previously turned any kernel exception into
    # an infinite trap loop. Hardware raises FS to Dirty on first FP use.
    li   t0, 0x2000
    csrrs zero, sstatus, t0

    # Zero BSS
    la  t0, __bss_start
    la  t1, __bss_end
1:  bge t0, t1, 2f
    sd  zero, (t0)
    addi t0, t0, 8
    j   1b
2:

    # Boot by hart 0 only (UEFI guarantees this)
    la  t0, boot_hartid
    sd  zero, (t0)

    # Install trap vector (direct mode)
    la  t0, trap_entry
    csrw stvec, t0

    # sscratch points to a dedicated trap stack, NOT the main kernel stack.
    # After the scheduler starts, sscratch is switched to the current
    # thread's own kernel stack on every restore (see context.S).
    la  t0, _trap_stack_top
    csrw sscratch, t0

    # Interrupts enabled later by Partic after all init is done

    # Pass BootInfo* to kernel_entry
    mv  a0, s0
    call kernel_entry
    wfi
    j   .

# ── Trap entry ──────────────────────────────────
# Saves a full 272-byte frame (30 GP regs + sepc + sstatus + saved_sp +
# kernel_stack_top) on the thread's kernel stack, calls trap_dispatch
# (returns the frame to restore from, possibly another thread's), then
# resumes through the shared restore_context tail (context.S).
.section .text
.align 4
trap_entry:
    # Atomically swap sp and sscratch
    # sp -> sscratch (interrupted_sp saved), sscratch -> sp (trap stack top)
    csrrw sp, sscratch, sp

    # Save all registers to trap frame (skip x0)
    addi sp, sp, -272

    sd  ra,   0(sp)
    sd  gp,   8(sp)
    sd  tp,  16(sp)
    sd  t0,  24(sp)
    sd  t1,  32(sp)
    sd  t2,  40(sp)
    sd  s0,  48(sp)
    sd  s1,  56(sp)
    sd  a0,  64(sp)
    sd  a1,  72(sp)
    sd  a2,  80(sp)
    sd  a3,  88(sp)
    sd  a4,  96(sp)
    sd  a5, 104(sp)
    sd  a6, 112(sp)
    sd  a7, 120(sp)
    sd  s2, 128(sp)
    sd  s3, 136(sp)
    sd  s4, 144(sp)
    sd  s5, 152(sp)
    sd  s6, 160(sp)
    sd  s7, 168(sp)
    sd  s8, 176(sp)
    sd  s9, 184(sp)
    sd  s10, 192(sp)
    sd  s11, 200(sp)
    sd  t3,  208(sp)
    sd  t4,  216(sp)
    sd  t5,  224(sp)
    sd  t6,  232(sp)

    csrr t0, sepc
    sd   t0, 240(sp)
    csrr t0, sstatus
    sd   t0, 248(sp)
    csrr t0, sscratch          # interrupted sp (from the swap above)
    sd   t0, 256(sp)
    addi t0, sp, 272           # kernel stack top (sp was top - 272)
    sd   t0, 264(sp)

    # a0 = scause, a1 = sepc, a2 = trap frame
    csrr a0, scause
    csrr a1, sepc
    mv   a2, sp
    # 置 mstatus.SUM（bit 18）：S-mode 访问用户页必需（syscall 读写用户内存）
    li   t0, 0x40000
    csrrs zero, sstatus, t0
    call trap_dispatch

    # a0 = frame to restore (same frame if no context switch)
    mv   sp, a0
    j    restore_context

boot_hartid:
    .space 8

.section .bss
.align 16
_stack_bottom:
    .space 1048576
_stack_top:

# Dedicated trap/interrupt stack (16 KiB), used before the scheduler starts.
.align 16
_trap_stack_bottom:
    .space 16384
_trap_stack_top:
