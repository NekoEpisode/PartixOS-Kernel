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
    # Deep interrupt handling (Partic dispatch -> EventBus -> StrBuilder/UART
    # printing) uses several KB of stack; sharing the main stack would
    # clobber the interrupted frame's locals (e.g. BspKrMain's allDevices).
    la  t0, _trap_stack_top
    csrw sscratch, t0

    # Interrupts enabled later by Partic after all init is done

    # Pass BootInfo* to kernel_entry
    mv  a0, s0
    call kernel_entry
    wfi
    j   .

# ── Trap entry ──────────────────────────────────
.section .text
.align 4
trap_entry:
    # Atomically swap sp and sscratch
    # sp -> sscratch (interrupted_sp saved), sscratch -> sp (trap stack top)
    csrrw sp, sscratch, sp

    # Save all registers to trap frame (skip x0)
    addi sp, sp, -256

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
    sd  a5,  104(sp)
    sd  a6,  112(sp)
    sd  a7,  120(sp)
    sd  s2,  128(sp)
    sd  s3,  136(sp)
    sd  s4,  144(sp)
    sd  s5,  152(sp)
    sd  s6,  160(sp)
    sd  s7,  168(sp)
    sd  s8,  176(sp)
    sd  s9,  184(sp)
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

    # a0 = scause, a1 = sepc, a2 = trap frame
    csrr a0, scause
    csrr a1, sepc
    mv   a2, sp
    call trap_dispatch

    # Restore
    ld   t0, 248(sp)
    csrw sstatus, t0
    ld   t0, 240(sp)
    csrw sepc, t0

    ld   ra,   0(sp)
    ld   gp,   8(sp)
    ld   tp,  16(sp)
    ld   t0,  24(sp)
    ld   t1,  32(sp)
    ld   t2,  40(sp)
    ld   s0,  48(sp)
    ld   s1,  56(sp)
    ld   a0,  64(sp)
    ld   a1,  72(sp)
    ld   a2,  80(sp)
    ld   a3,  88(sp)
    ld   a4,  96(sp)
    ld   a5,  104(sp)
    ld   a6,  112(sp)
    ld   a7,  120(sp)
    ld   s2,  128(sp)
    ld   s3,  136(sp)
    ld   s4,  144(sp)
    ld   s5,  152(sp)
    ld   s6,  160(sp)
    ld   s7,  168(sp)
    ld   s8,  176(sp)
    ld   s9,  184(sp)
    ld   s10, 192(sp)
    ld   s11, 200(sp)
    ld   t3,  208(sp)
    ld   t4,  216(sp)
    ld   t5,  224(sp)
    ld   t6,  232(sp)

    addi sp, sp, 256
    # Restore interrupted sp from sscratch, save trap sp back
    csrrw sp, sscratch, sp
    sret

boot_hartid:
    .space 8

.section .bss
.align 16
_stack_bottom:
    .space 1048576
_stack_top:

# Dedicated trap/interrupt stack (16 KiB).
.align 16
_trap_stack_bottom:
    .space 16384
_trap_stack_top:
