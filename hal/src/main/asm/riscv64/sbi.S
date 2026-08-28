// RISC-V SBI calls (hand-written to guarantee exact a0-a7 register state).
//
// The C compiler's inline asm cannot reliably force values into specific
// argument registers for an ecall, which made SBI set_timer fail / hang on
// QEMU+OpenSBI. Hand-written assembly removes that uncertainty: these
// functions follow the standard S-mode C calling convention (arg0 in a0),
// set the SBI extension registers explicitly, and return a0 as the SBI
// error code (0 on success).
//
// NOTE: this file is preprocessed (.S), so all comments use '//' — a line
// starting with '# error' would be treated as the #error directive.

.section .text

// long sbi_set_timer(uint64_t stime_value)
// SBI v0.2+ TIME extension: eid = 0x54494D45, fid = 0
.globl sbi_set_timer
.type  sbi_set_timer, @function
sbi_set_timer:
    li  a6, 0x54494D45        // SBI_EXT_TIME
    li  a7, 0                 // fid: sbi_set_timer
    ecall
    ret

// long sbi_set_timer_legacy(uint64_t stime_value)
// Legacy SBI v0.1: a7 = 0 (SBI_SET_TIMER)
.globl sbi_set_timer_legacy
.type  sbi_set_timer_legacy, @function
sbi_set_timer_legacy:
    li  a7, 0                 // legacy SBI_SET_TIMER
    ecall
    ret

// void sbi_console_putchar(int c)
// Legacy SBI v0.1: a7 = 1 (SBI_CONSOLE_PUTCHAR), a0 = char.
// Used by BootFail: the platform UART address may be unknown (no FDT),
// but an SBI-conformant M-mode firmware (OpenSBI) always provides this.
.globl sbi_console_putchar
.type  sbi_console_putchar, @function
sbi_console_putchar:
    li  a7, 1                 // legacy SBI_CONSOLE_PUTCHAR
    ecall
    ret

.section .note.GNU-stack,"",@progbits
