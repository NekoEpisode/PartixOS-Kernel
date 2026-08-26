#include <stdint.h>
#include "../runtime/timer_config.h"
#include "../runtime/allocator.h"

// Heap region for the slab allocator (runtime/allocator.c).
uint64_t kr_heap_start = 0x81000000;
uint64_t kr_heap_end  = 0x82000000;

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
