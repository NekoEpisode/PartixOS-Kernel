# x86_64 UEFI kernel entry
# RCX = BootInfo* (MS ABI first arg)

.code64
.section .text
.globl kern_entry

kern_entry:
    push %rcx

    lea __bss_start(%rip), %rdi
    lea __bss_end(%rip), %rcx
    sub %rdi, %rcx
    xor %eax, %eax
    rep stosb

    pop %rcx
    mov %rcx, %rdi
    mov $0x300000, %rsp
    cli
    call kernel_entry

halt:
    cli
    hlt
    jmp halt
