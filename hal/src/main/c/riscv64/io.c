#include <stdint.h>

void enableInterrupts() {
    // STIE | SEIE. Deliberately no SSIE: the kernel has no IPI source yet,
    // and an inherited/leftover SSIP would otherwise trap forever.
    __asm__ volatile("csrw sie, %0" :: "r"(0x220));
    __asm__ volatile("csrsi sstatus, 0x2");
}

void stopInterrupts() {
    // 只关全局中断门控（sstatus.SIE），不动 sie（各中断源使能）。
    // 线程切换依赖此语义：切换后由 sret 恢复 sstatus(SPIE=1) 重新开中断；
    // 若清掉 sie，定时器等源将永久失效。
    __asm__ volatile("csrci sstatus, 0x2");
}

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
