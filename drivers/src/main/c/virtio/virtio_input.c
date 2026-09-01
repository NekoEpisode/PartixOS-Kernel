// virtio-input device driver (keyboard).
//
// Reuses the common virtqueue + virtio-pci transport (virtio_queue.c /
// virtio_pci.c). The input device has two virtqueues: 0 = eventq (device ->
// driver, input events), 1 = statusq (driver -> device, LED feedback —
// skipped for the keyboard). No feature bits.
//
// Events are 8-byte virtio_input_event { le16 type; le16 code; le32 value; }
// where code is the Linux evdev keycode (KEY_A=30, ...). We copy them into
// an in-driver ring buffer; the Partic layer polls via virtio_input_poll and
// forwards to /dev/kbd. Keycode -> character mapping is deliberately left to
// user space.

#include "virtio_queue.h"
#include "virtio_pci.h"
#include <stdint.h>
#include "mem_layout.h"

#define VIRTIO_INPUT_EVENT_SIZE 8
#define VIRTIO_INPUT_RING_SIZE  64

typedef struct virtio_input_dev {
    virtq_t eventq;
    // common/notify 都只经 virtio_pci_* 访问（其内部转 volatile），
    // 成员不加 volatile，避免 volatile→void* 传参丢限定符告警。
    virtio_pci_common_cfg_t *common;
    volatile void *notify;
    uint16_t queue_notify_off;

    // event ring (raw virtio_input_event copies)
    uint8_t ring[VIRTIO_INPUT_RING_SIZE][VIRTIO_INPUT_EVENT_SIZE];
    volatile uint16_t ring_head;   // written by poll
    volatile uint16_t ring_tail;   // consumed by partic
} virtio_input_dev_t;

int virtio_input_dev_size(void) {
    return (int)sizeof(virtio_input_dev_t);
}

// Buffers offered to the device (device-writable, eventq). One buffer per
// descriptor, each pointing at a distinct ring slot. queue_size must be
// <= VIRTIO_INPUT_RING_SIZE (driver clamps it); after the device consumes a
// buffer we re-offer the same slot, so slots always map 1:1 to descriptors.
//
// descriptor addr 需要物理地址：dev->ring 在堆（VA），转 physmap 物理。
static void eventq_fill(virtio_input_dev_t *dev, virtq_t *q) {
    uint16_t n = q->queue_size;
    for (uint16_t i = 0; i < n; i++) {
        uint16_t d = virtq_alloc_desc(q);
        uint64_t phys = virt_to_phys((uint64_t)(uintptr_t)dev->ring[i]);
        virtq_fill_desc(q, d, phys, VIRTIO_INPUT_EVENT_SIZE, VIRTQ_DESC_F_WRITE);
        virtq_submit(q, d);
    }
}

void virtio_input_dev_init(virtio_input_dev_t *dev,
                           uint64_t common_phys, uint64_t notify_phys,
                           uint64_t desc_phys, uint64_t avail_phys,
                           uint64_t used_phys, uint16_t queue_size) {
    dev->common = (virtio_pci_common_cfg_t *)(uint64_t)common_phys;
    dev->notify = (volatile void *)(uint64_t)notify_phys;
    dev->queue_notify_off = virtio_pci_get_queue_notify_off(dev->common);
    dev->ring_head = 0;
    dev->ring_tail = 0;

    virtq_init(&dev->eventq, desc_phys, avail_phys, used_phys, queue_size);
    eventq_fill(dev, &dev->eventq);
}

// Returns 1 and writes the raw 8-byte virtio_input_event to ev_out if an
// event is available, else 0. Polls the used ring (interrupt-free like the
// GPU path).
int virtio_input_poll(virtio_input_dev_t *dev, void *ev_out) {
    virtq_t *q = &dev->eventq;
    uint32_t len;
    int id = virtq_get_used(q, &len);
    if (id < 0) return 0;

    // The buffer the device just used corresponds to descriptor `id`, which
    // points at ring slot `id`. Copy the event out, then re-offer the slot.
    uint8_t *ev = dev->ring[id];
    for (int i = 0; i < VIRTIO_INPUT_EVENT_SIZE; i++) {
        ((uint8_t *)ev_out)[i] = ev[i];
    }

    virtq_fill_desc(q, (uint16_t)id, virt_to_phys((uint64_t)(uintptr_t)ev),
                    VIRTIO_INPUT_EVENT_SIZE, VIRTQ_DESC_F_WRITE);
    virtq_submit(q, (uint16_t)id);
    return 1;
}
