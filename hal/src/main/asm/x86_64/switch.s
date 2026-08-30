# x86_64 context switch machinery
#
# Frame layout (184 bytes, from frame base / rsp):
#   +0..112  r15 r14 r13 r12 r11 r10 r9 r8 rbp rdi rsi rdx rcx rbx rax
#   +120     vector
#   +128     errcode
#   +136     RIP
#   +144     CS
#   +152     RFLAGS
#   +160     RSP
#   +168     SS
#   +176     kernel stack top (for TSS.RSP0 later)
#
# thread_switch_frame: voluntary switch. Builds a full ISR-style frame for the
#   caller (RIP = resume label, IF set), stores it into thread_frames[tid],
#   then resumes new_frame through restore_frame.
# restore_frame: common restore tail, also used by isr.s isr_common.

.section .text

# void thread_switch_frame(long tid, long new_frame)  — SysV: rdi=tid, rsi=new_frame
.globl thread_switch_frame
thread_switch_frame:
    movq %rsp, %r11               # r11 = C (caller rsp; return address at C)
    movq %rdi, %r8                # r8  = tid
    movq %rsi, %r9                # r9  = new_frame

    # Reserve the caller's SysV red zone (128 bytes below its rsp) so the
    # frame we build below does not clobber it.
    subq $128, %rsp

    # ── raw 7 slots (pushed first → high end) ──
    xorq %rax, %rax
    mov  %ss, %ax
    pushq %rax                    # SS   (+168)
    pushq %r11                    # RSP  (+160) = C
    pushfq                        # RFLAGS (+152)
    orq   $0x200, (%rsp)          # ensure IF on resume
    xorq %rax, %rax
    mov  %cs, %ax
    pushq %rax                    # CS   (+144) = current kernel CS
    leaq 1f(%rip), %rax
    pushq %rax                    # RIP  (+136) = resume label
    pushq $0                      # errcode (+128)
    pushq $0xFE                   # vector  (+120)

    # ── GP regs, pushed in restore order: rax at +112 ... r15 at +0 ──
    pushq %rax                    # +112 (rax = resume label — harmless)
    pushq %rbx                    # +104
    pushq %rcx                    # +96
    pushq %rdx                    # +88
    pushq %rsi                    # +80 (new_frame — harmless)
    pushq %rdi                    # +72 (tid — harmless)
    pushq %rbp                    # +64
    pushq %r8                     # +56 (tid — harmless)
    pushq %r9                     # +48 (new_frame — harmless)
    pushq %r10                    # +40
    pushq %r11                    # +32 (r11 = C — harmless, caller-saved)
    pushq %r12                    # +24
    pushq %r13                    # +16
    pushq %r14                    # +8
    pushq %r15                    # +0

    # kstack_top at +176
    movq g_current_kstack_top(%rip), %rax
    movq %rax, 176(%rsp)

    # thread_frames[tid] = frame
    leaq thread_frames(%rip), %rax
    movq %r8, %rcx
    shlq $3, %rcx
    movq %rsp, %rdx
    movq %rdx, (%rax, %rcx)

    # resume new_frame
    movq %r9, %rsp
    jmp  restore_frame
1:
    ret

# ── Common restore tail ─────────────────────────
.globl restore_frame
restore_frame:
    # TSS.RSP0 = frame+176 (next thread's kernel stack top), so a ring3
    # interrupt/syscall from this thread lands on its own kernel stack.
    # Pass the frame BASE (gdt_set_rsp0_from_frame reads frame+176 itself).
    movq %rsp, %rdi
    call gdt_set_rsp0_from_frame
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    addq $16, %rsp                # skip vector + errcode
    iretq
