#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

void paging_load_cr3(uint64_t cr3);
uint64_t paging_read_cr2(void);

#endif
