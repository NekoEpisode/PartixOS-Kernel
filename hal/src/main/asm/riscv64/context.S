# RISC-V 64 context switch machinery
#
# Frame layout (272 bytes, 8-byte slots):
#   0..29  ra gp tp t0..t6 s0..s11 a0..a7   (30 GP regs)
#   30     sepc
#   31     sstatus
#   32     saved_sp        (the sp to resume with)
#   33     kernel_stack_top (sscratch value for this thread)
#
# thread_switch_frame: voluntary switch. Builds a full frame for the caller
#   (sepc = resume label, SPP/SPIE forced), stores it into thread_frames[tid],
#   then resumes new_frame through restore_context.
# restore_context: common restore tail, also used by entry.S trap_entry.

.section .text
.align 4

.globl thread_switch_frame
thread_switch_frame:
    addi sp, sp, -272
    sd   ra,   0(sp)
    sd   gp,   8(sp)
    sd   tp,  16(sp)
    sd   t0,  24(sp)
    sd   t1,  32(sp)
    sd   t2,  40(sp)
    sd   s0,  48(sp)
    sd   s1,  56(sp)
    sd   a0,  64(sp)
    sd   a1,  72(sp)
    sd   a2,  80(sp)
    sd   a3,  88(sp)
    sd   a4,  96(sp)
    sd   a5, 104(sp)
    sd   a6, 112(sp)
    sd   a7, 120(sp)
    sd   s2, 128(sp)
    sd   s3, 136(sp)
    sd   s4, 144(sp)
    sd   s5, 152(sp)
    sd   s6, 160(sp)
    sd   s7, 168(sp)
    sd   s8, 176(sp)
    sd   s9, 184(sp)
    sd   s10, 192(sp)
    sd   s11, 200(sp)
    sd   t3, 208(sp)
    sd   t4, 216(sp)
    sd   t5, 224(sp)
    sd   t6, 232(sp)

    # sepc = resume point
    la   t0, 1f
    sd   t0, 240(sp)
    # sstatus: keep current bits, force SPP (S-mode), SPIE (irqs on resume)
    # and SUM (S-mode may access user pages)
    csrr t0, sstatus
    li   t1, 0x40120
    or   t0, t0, t1
    sd   t0, 248(sp)
    # saved_sp = sp before the frame (caller's stack pointer)
    addi t0, sp, 272
    sd   t0, 256(sp)
    # kernel_stack_top = sscratch (this thread's trap stack top)
    csrr t0, sscratch
    sd   t0, 264(sp)

    # slot = &thread_frames[tid]; store the frame pointer
    la   t0, thread_frames
    slli t1, a0, 3
    add  t0, t0, t1
    mv   t1, sp
    sd   t1, 0(t0)

    # resume the new thread
    mv   sp, a1
    j    restore_context
1:
    ret

# ── Common restore tail ─────────────────────────
.globl restore_context
restore_context:
    ld   t0, 264(sp)
    csrw sscratch, t0
    ld   t0, 240(sp)
    csrw sepc, t0
    ld   t0, 248(sp)
    csrw sstatus, t0
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
    ld   a5, 104(sp)
    ld   a6, 112(sp)
    ld   a7, 120(sp)
    ld   s2, 128(sp)
    ld   s3, 136(sp)
    ld   s4, 144(sp)
    ld   s5, 152(sp)
    ld   s6, 160(sp)
    ld   s7, 168(sp)
    ld   s8, 176(sp)
    ld   s9, 184(sp)
    ld   s10, 192(sp)
    ld   s11, 200(sp)
    ld   t3, 208(sp)
    ld   t4, 216(sp)
    ld   t5, 224(sp)
    ld   t6, 232(sp)
    ld   sp, 256(sp)
    sret
