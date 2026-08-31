#include <stdint.h>
#include "../runtime/timer_config.h"
#include "../runtime/allocator.h"

// Heap region for the slab allocator (runtime/allocator.c).
// The bump region is a bootstrap area; the page allocator extends the heap
// past kr_heap_end once initialized (see runtime/page_alloc.c).
uint64_t kr_heap_start = 0x81000000;
uint64_t kr_heap_end  = 0x82000000;

// Kernel image span (linker symbols __kernel_image_start/end): occupied
// physical memory the page allocator must not hand out.
extern char __kernel_image_start[];
extern char __kernel_image_end[];
uint64_t kr_image_start = (uint64_t)__kernel_image_start;
uint64_t kr_image_end   = (uint64_t)__kernel_image_end;

// Boot stack region: no-op on RISC-V (the boot stack is in the image .bss).
uint64_t kr_stack_bottom = 0;
uint64_t kr_stack_top    = 0;

uint64_t kr_malloc(uint64_t size) { return kr_alloc(size); }
void kr_free(uint64_t addr) { kr_dealloc(addr); }

void* memcpy(void* dst, const void* src, unsigned long n) {
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    while (n--) *d++ = *s++;
    return dst;
}

void* memset(void* s, int c, unsigned long n) {
    unsigned char* p = (unsigned char*)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

void* memmove(void* dst, const void* src, unsigned long n) {
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    if (d < s) { while (n--) *d++ = *s++; }
    else { d += n; s += n; while (n--) *--d = *--s; }
    return dst;
}

int memcmp(const void* a, const void* b, unsigned long n) {
    const unsigned char* pa = (const unsigned char*)a;
    const unsigned char* pb = (const unsigned char*)b;
    while (n--) { if (*pa != *pb) return *pa - *pb; pa++; pb++; }
    return 0;
}

unsigned long strlen(const char* s) {
    unsigned long n = 0;
    while (*s++) n++;
    return n;
}

int strncmp(const char* a, const char* b, unsigned long n) {
    while (n-- && *a && *b) { if (*a != *b) return *a - *b; a++; b++; }
    return n == (unsigned long)-1 ? 0 : *a - *b;
}

int atoi(const char* s) {
    int v = 0;
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; }
    return v;
}

static volatile unsigned int _lwip_seed = 1;

int lwip_rand(void) {
    _lwip_seed = _lwip_seed * 1103515245 + 12345;
    return (int)(_lwip_seed >> 16);
}

extern volatile unsigned long g_tick;

unsigned long sys_now(void) {
    return g_tick * TIMER_TICK_MS;
}

unsigned int sys_arch_protect(void) { return 0; }
void sys_arch_unprotect(unsigned int lev) { (void)lev; }

// JH7110 L2 cache 控制器 flush（移植自 starfive-u-boot arch/riscv/cpu/jh7110/cache.c）
// JH7110 无 Zicbom cbo.clean，改走 L2 控制器 MMIO：往 0x2010200 写 cache line 地址即回写该行。
// GOP framebuffer 直映写后必须调用，否则显示控制器读 DRAM 是 cache 里的旧数据。
#define L2_CACHE_FLUSH64    0x200
#define L2_CACHE_BASE_ADDR  0x2010000
#define CACHELINE_SIZE      64

void cache_clean_range(uint64_t addr, uint64_t size) {
    if (size == 0) return;
    volatile unsigned long *flush64 =
        (volatile unsigned long *)(L2_CACHE_BASE_ADDR + L2_CACHE_FLUSH64);
    __asm__ volatile("fence iorw, iorw" ::: "memory");
    uint64_t line = addr & ~(uint64_t)(CACHELINE_SIZE - 1);
    uint64_t end = addr + size;
    for (; line < end; line += CACHELINE_SIZE) {
        *flush64 = line;
        __asm__ volatile("fence iorw, iorw" ::: "memory");
    }
    __asm__ volatile("fence iorw, iorw" ::: "memory");
}
