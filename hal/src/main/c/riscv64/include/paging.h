#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

void paging_load_satp(uint64_t satp);
uint64_t paging_read_stval(void);

#endif
