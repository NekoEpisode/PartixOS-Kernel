# RISC-V 64 user-mode test program (embedded, freestanding).
# Syscall ABI: a7 = number, a0..a5 = args, ecall; return in a0.
#   write(fd, buf, len) = 1, yield() = 2, sleep(ms) = 3, exit(code) = 0

.section .text
.globl _start
_start:
    # write(1, msg1, 32)
    li   a7, 1
    li   a0, 1
    la   a1, msg1
    li   a2, 32
    ecall

    # sleep(500)
    li   a7, 3
    li   a0, 500
    ecall

    # write(1, msg2, 30)
    li   a7, 1
    li   a0, 1
    la   a1, msg2
    li   a2, 30
    ecall

    # yield()
    li   a7, 2
    ecall

    # write(1, msg3, 43)
    li   a7, 1
    li   a0, 1
    la   a1, msg3
    li   a2, 43
    ecall

    # exit(0)
    li   a7, 0
    li   a0, 0
    ecall

1:  j    1b

.section .rodata
msg1:    .ascii "Hello from user mode (riscv64)!\n"    # 32 bytes
msg2:    .ascii "  [user] slept 500ms, resumed\n"     # 30 bytes
msg3:    .ascii "  [user] yielded, still alive; exiting now\n"   # 43 bytes
