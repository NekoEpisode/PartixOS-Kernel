# RISC-V 64 user-mode keyboard test (embedded, freestanding).
# Syscall ABI: a7 = number, a0..a5 = args, ecall; return in a0.
#   exit(code)=0, write(fd,buf,len)=1, yield()=2, sleep(ms)=3, gettid=4
#   open(path,mode)=5, read(fd,buf,len)=6
# 打开 /dev/input/event0 读原始事件（8 字节：type/code/value），按键打印 code 十六进制。

.section .text
.globl _start
_start:
    # write(1, msg_open, 38)
    li   a7, 1
    li   a0, 1
    la   a1, msg_open
    li   a2, 38
    ecall

    # open("/dev/input/event0", 1=READ)
    li   a7, 5
    la   a0, kbd_path
    li   a1, 1
    ecall
    mv   s0, a0              # s0 = fd
    # DEBUG: fd < 0 则打印并退出
    bgez s0, 1f
    li   a7, 1
    li   a0, 1
    la   a1, msg_fd
    li   a2, 12
    ecall
    li   a7, 0
    li   a0, 1
    ecall
1:

    # write(1, msg_read, 30)
    li   a7, 1
    li   a0, 1
    la   a1, msg_read
    li   a2, 30
    ecall

read_loop:
    # read(fd, evbuf, 8) —— 内核侧真阻塞（至多 100ms），无事件自动重试
    li   a7, 6
    mv   a0, s0
    la   a1, evbuf
    li   a2, 8
    ecall
    bltz a0, read_loop       # 错误 → 重试
    beqz a0, read_loop       # 超时无事件 → 重试

    # 只显示"按下"（value==1）：evbuf 布局 [type:2][code:2][value:4]
    # （SYN 等非 EV_KEY 事件已由内核驱动过滤，这里再按 value 过滤松开事件）
    la   t0, evbuf
    lbu  t1, 4(t0)
    li   t2, 1
    bne  t1, t2, read_loop   # 松开/其他 → 不显示

    # 打印 "  code=0x"
    li   a7, 1
    li   a0, 1
    la   a1, msg_code
    li   a2, 9
    ecall

    # 打印 code 高字节 (evbuf+3)
    la   t0, evbuf
    lbu  a0, 3(t0)
    call print_hex_byte
    # 打印 code 低字节 (evbuf+2)
    la   t0, evbuf
    lbu  a0, 2(t0)
    call print_hex_byte

    # 换行
    li   a7, 1
    li   a0, 1
    la   a1, msg_nl
    li   a2, 1
    ecall
    j    read_loop

# print_hex_byte: a0 = byte，用 write 逐字符打印 "XX"
print_hex_byte:
    addi sp, sp, -16
    sd   ra, 8(sp)
    sd   s1, 0(sp)

    mv   s1, a0
    srli a0, s1, 4
    call print_nibble
    mv   a0, s1
    call print_nibble

    ld   ra, 8(sp)
    ld   s1, 0(sp)
    addi sp, sp, 16
    ret

# print_nibble: a0 = 0..15，打印单个 hex 字符
print_nibble:
    addi sp, sp, -16
    sd   ra, 8(sp)
    andi a0, a0, 15
    li   t0, 10
    blt  a0, t0, 1f
    addi a0, a0, 87        # 'a'-10
    j    2f
1:  addi a0, a0, 48        # '0'
2:  la   t0, hexbuf
    sb   a0, 0(t0)
    li   a7, 1
    li   a0, 1
    mv   a1, t0
    li   a2, 1
    ecall
    ld   ra, 8(sp)
    addi sp, sp, 16
    ret

    # exit(0) (unreachable)
    li   a7, 0
    li   a0, 0
    ecall
1:  j    1b

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
