# RISC-V 64 kernel entry（双段：低地址引导区 + 高半区主映像）
#
# 引导流程（低地址 0x80200000，U-Boot/UEFI identity 下执行）：
#   1. 用低区静态数组构建 SV39 引导页表：identity（0..4GB）+ physmap
#      （KERNEL_PHYS_BASE + 0..4GB，pgd 槽 256 起）
#   2. 写 satp（SV39），跳到高半区 high_start
#   3. 高区：设 gp、清 BSS、换栈到高区引导栈、调 kernel_entry(BootInfo)
# 引导表只映射 4GB（QEMU/真机场景足够）；规范页表由 Partic 侧 ptm.init() 全量重建。

.section .text.entry
.globl kern_entry
.align 4

kern_entry:
    mv  s0, a0                    # 保存 BootInfo*（s0 全程不被破坏）

    # 关闭继承自 UEFI 的中断
    csrw sie, zero
    csrci sstatus, 0x2

    # 清零引导页表（静态区，物理==虚拟）；大小 = 4096 + 4*4096 + 4*4096 字节
    la   t0, early_pgd
    li   t1, 4608                     # 36864 字节 / 8
    li   t2, 0
1:  sd   t2, 0(t0)
    addi t0, t0, 8
    addi t1, t1, -1
    bnez t1, 1b

    # pgd[0..3] -> early_pmd_i[0..3]（identity，每 1GB 一个 pmd 表）
    la   t0, early_pgd
    la   t1, early_pmd_i
    li   t2, 4
    li   t6, 4096
2:  srli t3, t1, 2
    ori  t3, t3, 1                 # PTE_V
    sd   t3, 0(t0)
    addi t0, t0, 8
    add  t1, t1, t6
    addi t2, t2, -1
    bnez t2, 2b

    # pgd[256..259] -> early_pmd_p[0..3]（physmap 内核区，pgd 槽 256 起）
    la   t0, early_pgd
    li   t4, 2048               # pgd[256] 偏移（256 * 8）
    add  t0, t0, t4
    la   t1, early_pmd_p
    li   t2, 4
3:  srli t3, t1, 2
    ori  t3, t3, 1
    sd   t3, 0(t0)
    addi t0, t0, 8
    add  t1, t1, t6
    addi t2, t2, -1
    bnez t2, 3b

    # 填 8 个 pmd 表（2MB 大页叶子，表项内容 = 物理地址）
    # identity：early_pmd_i[g]，物理 g*1GB .. +1GB
    la   a0, early_pmd_i
    li   a1, 0
    call fill_pmd
    la   a0, early_pmd_i + 4096
    li   a1, 0x40000000
    call fill_pmd
    la   a0, early_pmd_i + 8192
    li   a1, 0x80000000
    call fill_pmd
    la   a0, early_pmd_i + 12288
    li   a1, 0xC0000000
    call fill_pmd
    # physmap：early_pmd_p[g]，表项同为物理（仅挂在 pgd 高区槽下）
    la   a0, early_pmd_p
    li   a1, 0
    call fill_pmd
    la   a0, early_pmd_p + 4096
    li   a1, 0x40000000
    call fill_pmd
    la   a0, early_pmd_p + 8192
    li   a1, 0x80000000
    call fill_pmd
    la   a0, early_pmd_p + 12288
    li   a1, 0xC0000000
    call fill_pmd

    # satp = SV39 | early_pgd>>12（物理）
    la   t0, early_pgd
    srli t0, t0, 12
    li   t1, 8
    slli t1, t1, 60
    or   t0, t0, t1
    csrw satp, t0

    # 跳高半区（目标地址存于 .data.early 表，PC 相对加载）
    ld   t0, high_start_tab
    jr   t0

# a0 = pmd 表地址，a1 = 起始物理地址（2MB 对齐）；填 512 项 2MB 大页叶子。
fill_pmd:
    li   t0, 0                     # 表内索引
1:  srli t1, a1, 2
    li   t2, 0xEF                  # V|R|W|X|A|D
    or   t1, t1, t2
    sd   t1, 0(a0)
    addi a0, a0, 8
    li   t3, 0x200000
    add  a1, a1, t3
    addi t0, t0, 1
    li   t3, 512
    blt  t0, t3, 1b
    ret

# ── 引导页表静态区（低地址，零堆依赖） ──
.section .data.early
.balign 4096
early_pgd:
    .space 4096
early_pmd_i:
    .space 4096 * 4
early_pmd_p:
    .space 4096 * 4
.balign 8
high_start_tab:
    .dword high_start              # 高半区入口（重定位后为高区地址）

# ── 高半区主映像（运行前已切 satp，physmap 覆盖内核映像） ──
.section .text
.globl high_start
.align 4
high_start:
    la   gp, __global_pointer$
    # 启用 FPU：mstatus.FS = 01（与旧引导相同，见原入口注释）
    li   t0, 0x2000
    csrrs zero, sstatus, t0

    # 设置 trap 向量与 trap 栈（高区符号，PC 相对；必须在任何异常前完成，
    # 否则 stvec 仍是 U-Boot 的——内核 satp 下取 U-Boot 向量即 #PF 风暴）
    la   t0, trap_entry
    csrw stvec, t0
    la   t0, _trap_stack_top
    csrw sscratch, t0
    la   t0, boot_hartid
    sd   zero, 0(t0)

    # 清 BSS（高区符号，physmap 已映射）
    la   t0, __bss_start
    la   t1, __bss_end
1:  bge  t0, t1, 2f
    sd   zero, 0(t0)
    addi t0, t0, 8
    j    1b
2:

    # 换栈到高区引导栈，调 C kernel_entry（高区），s0 = BootInfo*
    la   sp, _boot_stack_top
    mv   a0, s0
    call kernel_entry
3:  wfi
    j    3b

# ── Trap entry ──────────────────────────────────
# Saves a full 272-byte frame (30 GP regs + sepc + sstatus + saved_sp +
# kernel_stack_top) on the thread's kernel stack, calls trap_dispatch
# (returns the frame to restore from, possibly another thread's), then
# resumes through the shared restore_context tail (context.S).
.align 4
trap_entry:
    # Atomically swap sp and sscratch
    # sp -> sscratch (interrupted_sp saved), sscratch -> sp (trap stack top)
    csrrw sp, sscratch, sp

    # Save all registers to trap frame (skip x0)
    addi sp, sp, -272

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
    sd  a5, 104(sp)
    sd  a6, 112(sp)
    sd  a7, 120(sp)
    sd  s2, 128(sp)
    sd  s3, 136(sp)
    sd  s4, 144(sp)
    sd  s5, 152(sp)
    sd  s6, 160(sp)
    sd  s7, 168(sp)
    sd  s8, 176(sp)
    sd  s9, 184(sp)
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
    csrr t0, sscratch          # interrupted sp (from the swap above)
    sd   t0, 256(sp)
    addi t0, sp, 272           # kernel stack top (sp was top - 272)
    sd   t0, 264(sp)

    # a0 = scause, a1 = sepc, a2 = trap frame
    csrr a0, scause
    csrr a1, sepc
    mv   a2, sp
    # 置 mstatus.SUM（bit 18）：S-mode 访问用户页必需（syscall 读写用户内存）
    li   t0, 0x40000
    csrrs zero, sstatus, t0
    call trap_dispatch

    # a0 = frame to restore (same frame if no context switch)
    mv   sp, a0
    j    restore_context

boot_hartid:
    .space 8

.section .bss
.align 16
_stack_bottom:
    .space 1048576
_stack_top:

# Dedicated trap/interrupt stack (16 KiB), used before the scheduler starts.
.align 16
_trap_stack_bottom:
    .space 16384
_trap_stack_top:
