OUTPUT_FORMAT("elf64-littleriscv")
OUTPUT_ARCH(riscv:rv64)
ENTRY(kern_entry)

/*
 * 双段链接：
 *   低地址引导区（0x80200000，物理==虚拟）——U-Boot/UEFI identity 映射下执行，
 *   负责构建引导页表（SV39）并切到高半区。
 *   高半区主映像（虚拟 = KERNEL_PHYS_BASE + 物理）——内核主体，LMA（物理）
 *   由 AT() 连续排布在引导区之后，bootloader 按 VMA - KERNEL_PHYS_BASE 加载。
 *   物理与虚拟的对应关系：主映像 VMA 起点 = KERNEL_PHYS_BASE + __early_phys_end，
 *   使每个段都满足 VMA = KERNEL_PHYS_BASE + LMA（physmap 偏移映射直接覆盖内核映像）。
 */

__kernel_phys_base = 0x80200000;

SECTIONS
{
    /* ── 低地址引导区（物理 == 虚拟） ── */
    . = 0x80200000;

    .text.entry : {
        KEEP(*(.text.entry))
    }

    .data.early : {
        *(.data.early)
    }
    __early_phys_end = ALIGN(., 0x1000);

    /* ── 高半区主映像（虚拟 = KERNEL_PHYS_BASE + 物理；起点与 __early_phys_end
     *    对齐，保证每段 VMA = BASE + LMA 恒成立；bootloader 按 VMA-BASE 加载，
     *    p_paddr 仅供参考） ── */
    . = 0xFFFFFFC000000000 + __early_phys_end;
    __kernel_image_start = .;

    .text : AT(__early_phys_end) {
        *(.text*)
    }

    .rodata : AT(ADDR(.text) + SIZEOF(.text) - 0xFFFFFFC000000000) {
        *(.rodata*)
        *(.lrodata .lrodata.*)
    }

    .data : AT(ADDR(.rodata) + SIZEOF(.rodata) - 0xFFFFFFC000000000) {
        *(.data*)
        . = ALIGN(8);
        /* riscv 小数据（.sdata/.sbss）：编译器按 small-data-limit 放小全局；
         * gp 相对寻址要求它们与 __global_pointer$ 距离在 ±2KB 内，故集中
         * 放一起并让 gp 指向 .sdata 起点 +0x800。 */
        __sdata_begin = .;
        *(.sdata*)
        *(.sdata2*)
        . = ALIGN(8);
        *(.sbss*)
        __sdata_end = .;
    }

    .eh_frame_hdr : AT(ADDR(.data) + SIZEOF(.data) - 0xFFFFFFC000000000) {
        __eh_frame_hdr_start = .;
        KEEP(*(.eh_frame_hdr))
        __eh_frame_hdr_end = .;
    }

    .eh_frame : AT(ADDR(.eh_frame_hdr) + SIZEOF(.eh_frame_hdr) - 0xFFFFFFC000000000) {
        __eh_frame_start = .;
        KEEP(*(.eh_frame))
        __eh_frame_end = .;
    }

    .gcc_except_table : AT(ADDR(.eh_frame) + SIZEOF(.eh_frame) - 0xFFFFFFC000000000) {
        *(.gcc_except_table .gcc_except_table.*)
    }

    .bss (NOLOAD) : AT(ADDR(.gcc_except_table) + SIZEOF(.gcc_except_table) - 0xFFFFFFC000000000) {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(16);
        _boot_stack_bottom = .;
        . += 65536;
        _boot_stack_top = .;
        __bss_end = .;
    }
    __kernel_image_end = .;

    .debug_info         0 : { KEEP(*(.debug_info .gnu.linkonce.wi.*)) }
    .debug_abbrev       0 : { KEEP(*(.debug_abbrev)) }
    .debug_line         0 : { KEEP(*(.debug_line .debug_line.*)) }
    .debug_str          0 : { KEEP(*(.debug_str)) }
    .debug_loc          0 : { KEEP(*(.debug_loc)) }
    .debug_ranges       0 : { KEEP(*(.debug_ranges)) }
    .debug_frame        0 : { KEEP(*(.debug_frame)) }
    .debug_aranges      0 : { KEEP(*(.debug_aranges)) }
    .debug_addr         0 : { KEEP(*(.debug_addr)) }
    .debug_line_str     0 : { KEEP(*(.debug_line_str)) }
    .debug_loclists     0 : { KEEP(*(.debug_loclists)) }
    .debug_rnglists     0 : { KEEP(*(.debug_rnglists)) }
    .debug_str_offsets  0 : { KEEP(*(.debug_str_offsets)) }

    /DISCARD/ : { *(.comment*) *(.note*) }

    __global_pointer$ = __sdata_begin + 0x800;
}
