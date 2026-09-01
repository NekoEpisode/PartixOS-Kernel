#include "virtio_queue.h"
#include <stdint.h>
#include "mem_layout.h"

#ifdef __riscv
#define VIRTIO_FENCE()   __asm__ volatile("fence w,o" ::: "memory")
#define VIRTIO_FENCE_R() __asm__ volatile("fence i,r" ::: "memory")
#else
#define VIRTIO_FENCE()   __asm__ volatile("" ::: "memory")
#define VIRTIO_FENCE_R() __asm__ volatile("" ::: "memory")
#endif

int virtq_struct_size(void) {
    return (int)sizeof(virtq_t);
}

// desc/avail/used 入参为物理地址（写 virtio 寄存器与 descriptor 用）；
// 访问环内存经 physmap 映射为 VA。

void virtq_init(virtq_t *q, uint64_t desc_phys, uint64_t avail_phys,
                uint64_t used_phys, uint16_t queue_size) {
    q->desc_phys  = desc_phys;
    q->avail_phys = avail_phys;
    q->used_phys  = used_phys;
    q->queue_size = queue_size;
    q->desc  = (virtq_desc_t  *)(uintptr_t)phys_to_virt(desc_phys);
    q->avail = (virtq_avail_t *)(uintptr_t)phys_to_virt(avail_phys);
    q->used  = (virtq_used_t  *)(uintptr_t)phys_to_virt(used_phys);
    q->last_used_idx = 0;
    q->free_desc = 0;
    q->queue_notify_off = 0;

    for (uint16_t i = 0; i < queue_size; i++) {
        q->desc[i].addr  = 0;
        q->desc[i].len   = 0;
        q->desc[i].flags = 0;
        q->desc[i].next  = i + 1;
    }
    q->desc[queue_size - 1].next = 0;
    q->avail->flags = 0;
    q->avail->idx   = 0;
    q->used->flags  = 0;
    q->used->idx    = 0;
}

uint16_t virtq_alloc_desc(virtq_t *q) {
    uint16_t d = q->free_desc;
    q->free_desc = q->desc[d].next;
    return d;
}

void virtq_fill_desc(virtq_t *q, uint16_t desc_idx,
                     uint64_t phys, uint32_t len, uint16_t flags) {
    q->desc[desc_idx].addr  = phys;
    q->desc[desc_idx].len   = len;
    q->desc[desc_idx].flags = flags;
    q->desc[desc_idx].next  = 0;
}

void virtq_submit(virtq_t *q, uint16_t desc_idx) {
    // avail->idx 是单调递增计数器（virtio 规范）：设备比较前后值判断
    // 是否有新描述符。这里不能取模后再 +1，否则 idx 会回绕、永远小于
    // queue_size，设备感知不到新提交。ring 下标才需要取模。
    uint16_t avail_idx = (uint16_t)(q->avail->idx % q->queue_size);
    q->avail->ring[avail_idx] = desc_idx;
    VIRTIO_FENCE();
    q->avail->idx = (uint16_t)(q->avail->idx + 1);
    VIRTIO_FENCE();
}

int virtq_get_used(virtq_t *q, uint32_t *len_out) {
    VIRTIO_FENCE_R();
    if (q->used->idx == q->last_used_idx) return -1;

    uint16_t used_idx = q->last_used_idx % q->queue_size;
    uint32_t id = q->used->ring[used_idx].id;
    if (len_out) *len_out = q->used->ring[used_idx].len;

    q->free_desc = id;
    q->last_used_idx++;
    return (int)id;
}
