OUTPUT_FORMAT("elf64-littleriscv")
OUTPUT_ARCH(riscv:rv64)
ENTRY(kern_entry)

SECTIONS
{
    . = 0x80200000;
    __kernel_image_start = .;

    .text : {
        *(.text.entry) *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .eh_frame_hdr : {
        __eh_frame_hdr_start = .;
        KEEP(*(.eh_frame_hdr))
        __eh_frame_hdr_end = .;
    }

    .eh_frame : {
        __eh_frame_start = .;
        KEEP(*(.eh_frame))
        __eh_frame_end = .;
    }

    .gcc_except_table : {
        *(.gcc_except_table .gcc_except_table.*)
    }

    .bss (NOLOAD) : {
        __bss_start = .;
        *(.sbss*)
        *(.bss*)
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

    __global_pointer$ = . + 0x800;
}
