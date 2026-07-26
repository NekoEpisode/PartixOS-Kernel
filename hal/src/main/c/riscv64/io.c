#include <stdint.h>

void write8(uint64_t addr, uint8_t data) {
    *(volatile uint8_t*)addr = data;
}
uint8_t read8(uint64_t addr) {
    return *(volatile uint8_t*)addr;
}

void write16(uint64_t addr, uint16_t data) {
    *(volatile uint16_t*)addr = data;
}
uint16_t read16(uint64_t addr) {
    return *(volatile uint16_t*)addr;
}

void write32(uint64_t addr, uint32_t data) {
    *(volatile uint32_t*)addr = data;
}
uint32_t read32(uint64_t addr) {
    return *(volatile uint32_t*)addr;
}

void write64(uint64_t addr, uint64_t data) {
    *(volatile uint64_t*)addr = data;
}
uint64_t read64(uint64_t addr) {
    return *(volatile uint64_t*)addr;
}

void halt() {
    __asm__ volatile ("wfi");
}
