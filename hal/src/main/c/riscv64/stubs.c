// Unused cross-arch symbols — only needed because
// Partic compiles all arch classes together and the
// linker needs every extern resolved.

#include "../runtime/hal_common.h"
#include <stdint.h>

void paging_load_cr3(uint64_t cr3) { (void)cr3; }
uint64_t paging_read_cr2(void) { return 0; }

// x86-only PS/2 keyboard; never instantiated on RISC-V (DevFSInit gates it
// on archId), but the Partic class is compiled for both arches.
int ps2kbd_init(void) { return -1; }
void ps2kbd_irq(void) { }
int ps2kbd_poll(void *ev_out) { (void)ev_out; return 0; }
