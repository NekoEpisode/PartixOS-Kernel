# x86_64 UEFI kernel entry（双段：低地址引导区 + 高半区主映像）
# RCX = BootInfo*（MS ABI first arg）
#
# 引导流程（全部在低地址、UEFI identity 映射下执行）：
#   1. 用低区静态数组构建引导页表：identity（0..4GB）+ physmap（KERNEL_PHYS_BASE+0..4GB）
#   2. 载 CR3，跳到高半区 high_start（physmap 已映射，代码继续在 identity 下可执行）
#   3. 高区：清 BSS、换栈到高区引导栈、调 C kernel_entry(BootInfo)
# 引导表只映射 4GB（QEMU 场景足够）；规范页表由 Partic 侧 ptm.init() 全量重建。

.code64

# ── 引导区（链接在 0x200000） ──
.section .text.entry
.globl kern_entry

kern_entry:
    mov %rcx, %r15                # 保存 BootInfo*（r15 在后续流程中不被破坏）

    cli

    # 清零引导页表（静态区，物理==虚拟）
    lea early_pml4(%rip), %rdi
    xor %eax, %eax
    mov $(EARLY_TABLES_BYTES / 8), %ecx
    rep stosq

    # PML4[0] -> identity PDPT；PML4[256] -> physmap PDPT
    lea early_pdpt_i(%rip), %rax
    or  $3, %rax
    mov %rax, early_pml4(%rip)
    lea early_pdpt_p(%rip), %rax
    or  $3, %rax
    mov %rax, early_pml4 + 256 * 8(%rip)

    # 两个 PDPT 各指向 4 个 PD 表（每 PD 覆盖 1GB）
    lea early_pd_i(%rip), %rsi
    lea early_pdpt_i(%rip), %rdi
    mov $4, %ecx
    call fill_pdpt
    lea early_pd_p(%rip), %rsi
    lea early_pdpt_p(%rip), %rdi
    mov $4, %ecx
    call fill_pdpt

    # identity PD：物理 0 .. 4GB（2MB 大页）
    lea early_pd_i(%rip), %rdi
    xor %esi, %esi
    mov $4, %ecx
    call fill_pds
    # physmap PD：表项同为物理 0 .. 4GB，挂在 PML4[256]（高区）下
    lea early_pd_p(%rip), %rdi
    xor %esi, %esi
    mov $4, %ecx
    call fill_pds

    # 切到引导页表（当前 RIP 在低区，identity 仍映射），跳高半区
    lea early_pml4(%rip), %rax
    mov %rax, %cr3
    movabs $high_start, %rax
    jmp *%rax

# rdi = pdpt 表地址，rsi = 第一个 pd 表地址，ecx = pd 表个数
fill_pdpt:
    xor %r8d, %r8d
1:  mov %rsi, %rax
    or  $3, %rax
    mov %rax, (%rdi, %r8, 8)
    add $4096, %rsi
    inc %r8d
    cmp %ecx, %r8d
    jb 1b
    ret

# rdi = 第一个 pd 表地址，rsi = 起始物理地址（2MB 对齐），ecx = pd 表个数
fill_pds:
    xor %r8d, %r8d                # pd 序号
1:  xor %r9d, %r9d                # 表内条目序号
2:  mov %rsi, %rax
    or  $0x83, %rax               # P|RW|PS（2MB 大页）
    mov %rax, (%rdi, %r9, 8)
    add $0x200000, %rsi
    inc %r9d
    cmp $512, %r9d
    jb 2b
    add $4096, %rdi
    inc %r8d
    cmp %ecx, %r8d
    jb 1b
    ret

# ── 引导页表静态区（低地址，零堆依赖） ──
.section .data.early
.align 4096
early_pml4:
    .space 4096
early_pdpt_i:
    .space 4096
early_pd_i:
    .space 4096 * 4
early_pdpt_p:
    .space 4096
early_pd_p:
    .space 4096 * 4
.equ EARLY_TABLES_BYTES, . - early_pml4

# ── 高半区主映像（链接在 KERNEL_PHYS_BASE + 0x200000，运行前已切 CR3） ──
.section .text
high_start:
    # 清 BSS（高区符号，physmap 已覆盖内核映像）
    movabs $__bss_start, %rdi
    movabs $__bss_end, %rcx
    sub %rdi, %rcx
    xor %eax, %eax
    rep stosb

    # 换栈到高区引导栈
    movabs $_boot_stack_top, %rsp

    # 调 C kernel_entry（高区），r15 = BootInfo*
    mov %r15, %rdi
    movabs $kernel_entry, %rax
    call *%rax

halt:
    cli
    hlt
    jmp halt
