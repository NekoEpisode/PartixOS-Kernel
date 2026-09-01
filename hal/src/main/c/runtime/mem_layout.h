#ifndef PARTIX_MEM_LAYOUT_H
#define PARTIX_MEM_LAYOUT_H

#if defined(__x86_64__) || defined(__i386__)
// x86_64：48-bit 高半区起点。
#define KERNEL_PHYS_BASE 0xFFFF800000000000ULL
#elif defined(__riscv)
// riscv64（SV39）：高 256GB 区起点。
#define KERNEL_PHYS_BASE 0xFFFFFFC000000000ULL
#else
#error "unsupported architecture for KERNEL_PHYS_BASE"
#endif

static inline unsigned long long phys_to_virt(unsigned long long phys) {
    return phys + KERNEL_PHYS_BASE;
}

static inline unsigned long long virt_to_phys(unsigned long long va) {
    return va - KERNEL_PHYS_BASE;
}

#endif /* PARTIX_MEM_LAYOUT_H */
