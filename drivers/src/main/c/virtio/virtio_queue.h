#ifndef VIRTIO_QUEUE_H
#define VIRTIO_QUEUE_H

#include <stdint.h>

#define VIRTQ_DESC_F_NEXT   1
#define VIRTQ_DESC_F_WRITE  2
#define VIRTQ_DESC_F_INDIRECT 4

typedef struct {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed)) virtq_desc_t;

typedef struct {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];
} virtq_avail_t;

typedef struct {
    uint16_t flags;
    uint16_t idx;
    struct {
        uint32_t id;
        uint32_t len;
    } ring[];
} virtq_used_t;

typedef struct {
    uint16_t queue_size;
    uint16_t queue_notify_off;
    uint64_t desc_phys;
    uint64_t avail_phys;
    uint64_t used_phys;
    virtq_desc_t  *desc;
    virtq_avail_t *avail;
    virtq_used_t  *used;
    uint16_t last_used_idx;
    uint16_t free_desc;
} virtq_t;

int virtq_struct_size(void);

void virtq_init(virtq_t *q, uint64_t desc_phys, uint64_t avail_phys,
                uint64_t used_phys, uint16_t queue_size);
uint16_t virtq_alloc_desc(virtq_t *q);
void virtq_fill_desc(virtq_t *q, uint16_t idx,
                     uint64_t phys, uint32_t len, uint16_t flags);
void virtq_submit(virtq_t *q, uint16_t idx);
int virtq_get_used(virtq_t *q, uint32_t *len_out);

#endif
