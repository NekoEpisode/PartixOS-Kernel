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

    # ── GP regs (r15 at +0 ... rax at +112) ──
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %r11                    # (r11 = C — harmless, caller-saved)
    pushq %r10
    pushq %r9                     # (r9 = new_frame — harmless)
    pushq %r8                     # (r8 = tid — harmless)
    pushq %rbp
    pushq %rdi                    # (tid)
    pushq %rsi                    # (new_frame)
    pushq %rdx
    pushq %rcx
    pushq %rbx
    pushq %rax                    # (rax = resume label — harmless)

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
    # (x86 milestone: update TSS.RSP0 from 176(%rsp) here)
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
