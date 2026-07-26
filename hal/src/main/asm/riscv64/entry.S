# RISC-V 64 kernel entry (UEFI S-mode)
.section .text.entry
.globl kern_entry
.align 4

kern_entry:
    mv  s0, a0

    la  sp, _stack_top
    la  gp, __global_pointer$

    la  t0, __bss_start
    la  t1, __bss_end
1:  bge t0, t1, 2f
    sd  zero, (t0)
    addi t0, t0, 8
    j   1b
2:

    la  t0, boot_hartid
    sd  zero, (t0)

    mv  a0, s0
    call kernel_entry
    wfi
    j .

.section .bss
.align 16
_stack_bottom:
    .space 1048576
_stack_top:

boot_hartid:
    .space 8
