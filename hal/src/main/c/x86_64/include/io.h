#ifndef IO_H
#define IO_H

#include "stdint.h"

// ============ 端口I/O操作 ============
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" :: "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" :: "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t val;
    __asm__ volatile("inw %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile("outl %0, %1" :: "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t val;
    __asm__ volatile("inl %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

// ============ 内存映射 vs 端口I/O 统一接口 ============
static inline int is_port(uint64_t addr) {
    return addr < 0x10000;
}

void write8(uint64_t addr, uint8_t data);
uint8_t read8(uint64_t addr);

void write16(uint64_t addr, uint16_t data);
uint16_t read16(uint64_t addr);

void write32(uint64_t addr, uint32_t data);
uint32_t read32(uint64_t addr);

void write64(uint64_t addr, uint64_t data);
uint64_t read64(uint64_t addr);

// ============ CPU控制 ============
static inline void halt(void) {
    __asm__ volatile("hlt");
}

static inline void enable_interrupts(void) {
    __asm__ volatile("sti");
}

static inline void disable_interrupts(void) {
    __asm__ volatile("cli");
}

static inline void nop(void) {
    __asm__ volatile("nop");
}

static inline void pause(void) {
    __asm__ volatile("pause");
}

static inline uint64_t read_cr2(void) {
    uint64_t val;
    __asm__ volatile("mov %%cr2, %0" : "=r"(val));
    return val;
}

static inline uint64_t read_cr3(void) {
    uint64_t val;
    __asm__ volatile("mov %%cr3, %0" : "=r"(val));
    return val;
}

static inline uint16_t read_cs(void) {
    uint16_t val;
    __asm__ volatile("mov %%cs, %0" : "=r"(val));
    return val;
}

#endif // IO_H