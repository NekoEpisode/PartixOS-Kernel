#include <stdint.h>
#include "../runtime/timer_config.h"
#include "../runtime/allocator.h"

// Heap region for the slab allocator (runtime/allocator.c).
// NOTE: must be large enough for the 1 TiB identity page tables
// (~8 MiB of 8192-byte PD allocations) plus kernel runtime objects.
uint64_t kr_heap_start = 0x2000000;
uint64_t kr_heap_end  = 0x4000000;   // 32 MiB

// Kernel image span (linker symbols __kernel_image_start/end): occupied
// physical memory the page allocator must not hand out.
extern char __kernel_image_start[];
extern char __kernel_image_end[];
uint64_t kr_image_start = (uint64_t)__kernel_image_start;
uint64_t kr_image_end   = (uint64_t)__kernel_image_end;

// Boot stack region: entry_uefi.s sets RSP to 0x300000; the stack grows down
// from there and must never be handed out as free memory. The region between
// the kernel image end and the stack top is reserved. (RISC-V: 0 = no-op,
// its boot stack lives inside the image .bss and is already carved.)
uint64_t kr_stack_bottom = (uint64_t)__kernel_image_end;
uint64_t kr_stack_top    = 0x300000;

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
    unsigned int lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    _lwip_seed = _lwip_seed * 1103515245 + 12345 + lo;
    return (int)(_lwip_seed >> 16);
}

unsigned long sys_now(void) {
    extern volatile unsigned long g_tick;
    return g_tick * TIMER_TICK_MS;
}

unsigned int sys_arch_protect(void) { return 0; }
void sys_arch_unprotect(unsigned int lev) { (void)lev; }

// GOP 直映 framebuffer 写后需回写 cache（clflush 按 64B cache line）。
// QEMU 无真实 cache 时 clflush 是安全 no-op。
void cache_clean_range(uint64_t addr, uint64_t size) {
    if (size == 0) return;
    uint64_t end = addr + size;
    uint64_t a = addr & ~(uint64_t)63;
    for (; a < end; a += 64) {
        __asm__ volatile("clflush (%0)" :: "r"(a) : "memory");
    }
    __asm__ volatile("mfence" ::: "memory");
}
