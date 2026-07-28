// Unused cross-arch symbols — only needed because
// Partic compiles all arch classes together and the
// linker needs every extern resolved.

#include "../runtime/hal_common.h"

void paging_load_cr3(uint64_t cr3) { (void)cr3; }
uint64_t paging_read_cr2(void) { return 0; }
