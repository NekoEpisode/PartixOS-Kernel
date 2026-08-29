# x86_64 user-mode test program (embedded, freestanding).
# Syscall ABI: eax = number, rdi/rsi/rdx/r10/r8/r9 = args, int $0x80.
#   write(fd, buf, len) = 1, yield() = 2, sleep(ms) = 3, exit(code) = 0

.code64
.section .text
.globl _start
_start:
    # write(1, msg1, 32)
    mov $1, %eax
    mov $1, %edi
    lea msg1(%rip), %rsi
    mov $32, %edx
    int $0x80

    # sleep(500)
    mov $3, %eax
    mov $500, %edi
    int $0x80

    # write(1, msg2, 30)
    mov $1, %eax
    mov $1, %edi
    lea msg2(%rip), %rsi
    mov $30, %edx
    int $0x80

    # yield()
    mov $2, %eax
    int $0x80

    # write(1, msg3, 43)
    mov $1, %eax
    mov $1, %edi
    lea msg3(%rip), %rsi
    mov $43, %edx
    int $0x80

    # exit(0)
    mov $0, %eax
    xor %edi, %edi
    int $0x80

1:  jmp 1b

.section .rodata
msg1:    .ascii "Hello from user mode (x86_64)!\n"     # 32 bytes
msg2:    .ascii "  [user] slept 500ms, resumed\n"      # 30 bytes
msg3:    .ascii "  [user] yielded, still alive; exiting now\n"   # 43 bytes
