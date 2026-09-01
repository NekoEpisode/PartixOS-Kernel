OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(kern_entry)

/*
 * 双段链接：
 *   低地址引导区（0x200000，物理==虚拟）——bootloader/UEFI identity 映射下执行，
 *   负责构建引导页表并切到高半区。
 *   高半区主映像（虚拟 = KERNEL_PHYS_BASE + 物理）——内核主体，LMA（物理）
 *   由 AT() 连续排布在引导区之后，bootloader 按 p_paddr 加载。
 *   物理与虚拟的对应关系：主映像 VMA 起点 = KERNEL_PHYS_BASE + __early_phys_end，
 *   使每个段都满足 VMA = KERNEL_PHYS_BASE + LMA（physmap 偏移映射直接覆盖内核映像）。
 */

__kernel_phys_base = 0x200000;

SECTIONS
{
    /* ── 低地址引导区（物理 == 虚拟） ── */
    . = 0x200000;

    .text.entry : {
        KEEP(*(.text.entry))
    }

    .data.early : {
        *(.data.early)
    }
    __early_phys_end = ALIGN(., 0x1000);

    /* ── 高半区主映像（虚拟 = KERNEL_PHYS_BASE + 物理；起点与 __early_phys_end 对齐，
     *    保证每段 VMA = BASE + LMA 恒成立，physmap 偏移映射可直接命中内核映像） ──
     *    各段 LMA 用 AT(ADDR(前段)+SIZEOF(前段)-BASE)：ld 对 VMA/LMA 按同一
     *    对齐规则推进，偏移恒等，避免手算与 ld 对齐不一致导致物理布局漂移。 */
    . = 0xFFFF800000000000 + __early_phys_end;
    __kernel_image_start = .;

    .text : AT(__early_phys_end) {
        *(.text .text.*)
        *(.ltext .ltext.*)
    }

    .rodata : AT(ADDR(.text) + SIZEOF(.text) - 0xFFFF800000000000) {
        *(.rodata .rodata.*)
        *(.lrodata .lrodata.*)
    }

    .data : AT(ADDR(.rodata) + SIZEOF(.rodata) - 0xFFFF800000000000) {
        *(.data .data.*)
        *(.ldata .ldata.*)
    }

    .eh_frame_hdr : AT(ADDR(.data) + SIZEOF(.data) - 0xFFFF800000000000) {
        __eh_frame_hdr_start = .;
        KEEP(*(.eh_frame_hdr))
        __eh_frame_hdr_end = .;
    }

    .eh_frame : AT(ADDR(.eh_frame_hdr) + SIZEOF(.eh_frame_hdr) - 0xFFFF800000000000) {
        __eh_frame_start = .;
        KEEP(*(.eh_frame))
        __eh_frame_end = .;
    }

    .gcc_except_table : AT(ADDR(.eh_frame) + SIZEOF(.eh_frame) - 0xFFFF800000000000) {
        *(.gcc_except_table .gcc_except_table.*)
    }

    .bss (NOLOAD) : AT(ADDR(.gcc_except_table) + SIZEOF(.gcc_except_table) - 0xFFFF800000000000) {
        __bss_start = .;
        *(.bss .bss.*)
        *(.lbss .lbss.*)
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
    .debug_macro        0 : { KEEP(*(.debug_macro)) }
    .debug_aranges      0 : { KEEP(*(.debug_aranges)) }
    .debug_addr         0 : { KEEP(*(.debug_addr)) }
    .debug_line_str     0 : { KEEP(*(.debug_line_str)) }
    .debug_loclists     0 : { KEEP(*(.debug_loclists)) }
    .debug_rnglists     0 : { KEEP(*(.debug_rnglists)) }
    .debug_str_offsets  0 : { KEEP(*(.debug_str_offsets)) }
    .debug*             0 : { KEEP(*(.debug*)) }
}
