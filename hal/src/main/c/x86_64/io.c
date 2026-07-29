#include "include/stdint.h"

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

static inline int is_port(uint64_t addr) {
    return addr < 0x10000;
}

void write8(uint64_t addr, uint8_t data) {
    if (is_port(addr)) { outb((uint16_t)addr, data); return; }
    *(volatile uint8_t*)addr = data;
}
uint8_t read8(uint64_t addr) {
    if (is_port(addr)) return inb((uint16_t)addr);
    return *(volatile uint8_t*)addr;
}

void write16(uint64_t addr, uint16_t data) {
    if (is_port(addr)) { outw((uint16_t)addr, data); return; }
    *(volatile uint16_t*)addr = data;
}
uint16_t read16(uint64_t addr) {
    if (is_port(addr)) return inw((uint16_t)addr);
    return *(volatile uint16_t*)addr;
}

void write32(uint64_t addr, uint32_t data) {
    if (is_port(addr)) { outl((uint16_t)addr, data); return; }
    *(volatile uint32_t*)addr = data;
}
uint32_t read32(uint64_t addr) {
    if (is_port(addr)) return inl((uint16_t)addr);
    return *(volatile uint32_t*)addr;
}

void write64(uint64_t addr, uint64_t data) {
    *(volatile uint64_t*)addr = data;
}
uint64_t read64(uint64_t addr) {
    return *(volatile uint64_t*)addr;
}

void halt() {
    __asm__ volatile ("hlt");
}

void enableInterrupts() {
    __asm__ volatile ("sti");
}

void stopInterrupts() {
     __asm__ volatile ("cli");
}
