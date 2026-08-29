OUTPUT_FORMAT("elf64-littleriscv")
ENTRY(_start)

SECTIONS
{
    . = 0x400000;
    .text : {
        *(.text*)
    }
    .rodata : ALIGN(4096) {
        *(.rodata*)
    }
    .data : {
        *(.data*)
    }
    .bss : {
        *(.bss*)
    }
}
