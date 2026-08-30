# x86_64 user-mode keyboard test (embedded, freestanding).
# Syscall ABI: eax = number, rdi/rsi/rdx/r10/r8/r9 = args, via `syscall`.
#   exit(code)=0, write(fd,buf,len)=1, yield()=2, sleep(ms)=3, gettid=4
#   open(path,mode)=5, read(fd,buf,len)=6
# 打开 /dev/input/event0 读原始事件（8 字节：type/code/value），
# 只显示"按下"（value==1）的 keycode 十六进制（SYN/松开由驱动/本程序过滤）。

.code64
.section .text
.globl _start
_start:
    # write(1, msg_open, 38)
    mov $1, %eax
    mov $1, %edi
    lea msg_open(%rip), %rsi
    mov $38, %edx
    syscall

    # open("/dev/input/event0", 1=READ)
    mov $5, %eax
    lea kbd_path(%rip), %rdi
    mov $1, %esi
    syscall
    mov %eax, %ebx              # ebx = fd（syscall 不破坏 callee-saved）
    test %eax, %eax
    js  open_failed

    # write(1, msg_read, 30)
    mov $1, %eax
    mov $1, %edi
    lea msg_read(%rip), %rsi
    mov $30, %edx
    syscall

read_loop:
    # read(fd, evbuf, 8) —— 内核侧真阻塞（至多 100ms），无事件自动重试
    mov $6, %eax
    mov %ebx, %edi
    lea evbuf(%rip), %rsi
    mov $8, %edx
    syscall
    test %eax, %eax
    jle read_loop               # 错误/超时 → 重试

    # 只显示"按下"（value==1）：evbuf 布局 [type:2][code:2][value:4]
    # （SYN 等非 EV_KEY 事件已由内核驱动过滤，这里再按 value 过滤松开事件）
    movzbl evbuf+4(%rip), %eax
    cmp $1, %eax
    jne read_loop               # 松开/其他 → 不显示

    # write(1, "  code=0x", 9)
    mov $1, %eax
    mov $1, %edi
    lea msg_code(%rip), %rsi
    mov $9, %edx
    syscall

    # 打印 code 高字节 (evbuf+3)
    movzbl evbuf+3(%rip), %eax
    call print_hex_byte
    # 打印 code 低字节 (evbuf+2)
    movzbl evbuf+2(%rip), %eax
    call print_hex_byte

    # write(1, "\n", 1)
    mov $1, %eax
    mov $1, %edi
    lea msg_nl(%rip), %rsi
    mov $1, %edx
    syscall
    jmp read_loop

open_failed:
    # write(1, msg_fd, 12)
    mov $1, %eax
    mov $1, %edi
    lea msg_fd(%rip), %rsi
    mov $12, %edx
    syscall
    # exit(1)
    mov $0, %eax
    mov $1, %edi
    syscall

1:  jmp 1b

# print_hex_byte: al = byte，打印两个 hex 字符（write 每次 1 字符）
print_hex_byte:
    push %r12
    mov %al, %r12b
    mov %r12b, %al
    shr $4, %al
    call print_nibble
    mov %r12b, %al
    call print_nibble
    pop %r12
    ret

# print_nibble: al = 0..15，打印单个 hex 字符
print_nibble:
    and $0x0f, %al
    cmp $10, %al
    jb  1f
    add $87, %al               # 'a'-10
    jmp 2f
1:  add $48, %al               # '0'
2:  lea hexbuf(%rip), %rsi
    mov %al, (%rsi)
    mov $1, %eax
    mov $1, %edi
    mov $1, %edx
    syscall
    ret

.section .rodata
msg_open:  .ascii "keyboard test: open /dev/input/event0\n"
kbd_path:  .ascii "/dev/input/event0\0"
msg_fd:    .ascii "open failed\n"
msg_read:  .ascii "  reading keys (press any)...\n"
msg_code:  .ascii "  code=0x"
msg_nl:    .ascii "\n"

.section .bss
.align 2
evbuf:     .space 8
hexbuf:    .space 2
