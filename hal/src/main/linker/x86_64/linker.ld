OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(kern_entry)

SECTIONS
{
    . = 0x200000;

    .text : {
        *(.text .text.*)
    }

    .rodata : {
        *(.rodata .rodata.*)
        KEEP(*(.data))
    }

    .data : {
        *(.data .data.*)
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

    .bss : {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    }

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
