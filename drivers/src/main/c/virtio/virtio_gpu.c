#include "virtio_gpu.h"
#include "virtio_queue.h"
#include "virtio_pci.h"
#include <stdint.h>

typedef struct virtio_gpu_dev {
    virtq_t          controlq;
    volatile virtio_pci_common_cfg_t *common;
    volatile void   *notify;
    volatile void   *device;
    uint64_t         cmdbuf_phys;
    virtio_gpu_ctrl_hdr_t *cmdbuf;
    uint64_t         respbuf_phys;
    virtio_gpu_ctrl_hdr_t *respbuf;
} virtio_gpu_dev_t;

int virtio_gpu_dev_size(void) {
    return (int)sizeof(virtio_gpu_dev_t);
}

static int gpu_timeout_count = 0;

int virtio_gpu_timeout_count(void) {
    return gpu_timeout_count;
}

void virtio_gpu_dev_init(virtio_gpu_dev_t *dev,
                         uint64_t common_phys, uint64_t notify_phys,
                         uint64_t device_phys,
                         uint64_t cmdbuf_phys, uint64_t respbuf_phys) {
    dev->common = (volatile virtio_pci_common_cfg_t *)(uint64_t)common_phys;
    dev->notify = (volatile void *)(uint64_t)notify_phys;
    dev->device = (volatile void *)(uint64_t)device_phys;
    dev->cmdbuf_phys = cmdbuf_phys;
    dev->cmdbuf = (virtio_gpu_ctrl_hdr_t *)(uint64_t)cmdbuf_phys;
    dev->respbuf_phys = respbuf_phys;
    dev->respbuf = (virtio_gpu_ctrl_hdr_t *)(uint64_t)respbuf_phys;
}

static int send_cmd(virtio_gpu_dev_t *dev, uint32_t cmd_type,
                    uint32_t cmd_size, uint32_t resp_size) {
    virtq_t *q = &dev->controlq;

    dev->cmdbuf->type    = cmd_type;
    dev->cmdbuf->flags   = 0;
    dev->cmdbuf->fence_id = 0;
    dev->cmdbuf->ctx_id  = 0;
    dev->cmdbuf->padding = 0;

    uint16_t d0 = virtq_alloc_desc(q);
    virtq_fill_desc(q, d0, dev->cmdbuf_phys, cmd_size, 0);

    uint16_t d1 = virtq_alloc_desc(q);
    virtq_fill_desc(q, d1, dev->respbuf_phys, resp_size, VIRTQ_DESC_F_WRITE);
    q->desc[d0].next = d1;

    virtq_submit(q, d0);

    uint16_t notify_off = q->queue_notify_off;
    volatile uint32_t *n = (volatile uint32_t *)((uint8_t *)dev->notify + notify_off);
    *n = 0;

    // 等设备响应 used ring；超时（约 1 秒，200Hz tick）返回错误而不是死循环
    extern volatile unsigned long g_tick;
    unsigned long deadline = g_tick + 200;
    while (1) {
        uint32_t len;
        int id = virtq_get_used(q, &len);
        if (id >= 0) break;
        if (g_tick >= deadline) {
            gpu_timeout_count++;
            return -1;
        }
    }

    return (int)dev->respbuf->type;
}

void virtio_gpu_create_2d(virtio_gpu_dev_t *dev, uint32_t id, uint32_t format,
                          uint32_t width, uint32_t height) {
    virtio_gpu_resource_create_2d_t *cmd =
        (virtio_gpu_resource_create_2d_t *)(uint64_t)dev->cmdbuf_phys;
    cmd->resource_id = id;
    cmd->format      = format;
    cmd->width       = width;
    cmd->height      = height;
    send_cmd(dev, VIRTIO_GPU_CMD_RESOURCE_CREATE_2D,
             sizeof(virtio_gpu_resource_create_2d_t),
             sizeof(virtio_gpu_ctrl_hdr_t));
}

void virtio_gpu_attach_backing(virtio_gpu_dev_t *dev, uint32_t id,
                               uint64_t phys, uint32_t size) {
    virtio_gpu_resource_attach_backing_t *cmd =
        (virtio_gpu_resource_attach_backing_t *)(uint64_t)dev->cmdbuf_phys;
    cmd->resource_id = id;
    cmd->nr_entries  = 1;
    cmd->entries[0].addr   = phys;
    cmd->entries[0].length = size;
    cmd->entries[0].padding = 0;
    send_cmd(dev, VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING,
             sizeof(virtio_gpu_resource_attach_backing_t) + sizeof(cmd->entries[0]),
             sizeof(virtio_gpu_ctrl_hdr_t));
}

void virtio_gpu_set_scanout(virtio_gpu_dev_t *dev, uint32_t scanout,
                            uint32_t id, uint32_t width, uint32_t height) {
    virtio_gpu_set_scanout_t *cmd =
        (virtio_gpu_set_scanout_t *)(uint64_t)dev->cmdbuf_phys;
    cmd->r.x          = 0;
    cmd->r.y          = 0;
    cmd->r.width      = width;
    cmd->r.height     = height;
    cmd->scanout_id   = scanout;
    cmd->resource_id  = id;
    send_cmd(dev, VIRTIO_GPU_CMD_SET_SCANOUT,
             sizeof(virtio_gpu_set_scanout_t),
             sizeof(virtio_gpu_ctrl_hdr_t));
}

void virtio_gpu_transfer_to_host_2d(virtio_gpu_dev_t *dev, uint32_t id,
                                    uint32_t x, uint32_t y,
                                    uint32_t width, uint32_t height) {
    virtio_gpu_transfer_to_host_2d_t *cmd =
        (virtio_gpu_transfer_to_host_2d_t *)(uint64_t)dev->cmdbuf_phys;
    cmd->r.x          = x;
    cmd->r.y          = y;
    cmd->r.width      = width;
    cmd->r.height     = height;
    cmd->offset       = 0;
    cmd->resource_id  = id;
    cmd->padding      = 0;
    send_cmd(dev, VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D,
             sizeof(virtio_gpu_transfer_to_host_2d_t),
             sizeof(virtio_gpu_ctrl_hdr_t));
}

void virtio_gpu_flush(virtio_gpu_dev_t *dev, uint32_t id,
                      uint32_t x, uint32_t y,
                      uint32_t width, uint32_t height) {
    virtio_gpu_resource_flush_t *cmd =
        (virtio_gpu_resource_flush_t *)(uint64_t)dev->cmdbuf_phys;
    cmd->r.x          = x;
    cmd->r.y          = y;
    cmd->r.width      = width;
    cmd->r.height     = height;
    cmd->resource_id  = id;
    cmd->padding      = 0;
    send_cmd(dev, VIRTIO_GPU_CMD_RESOURCE_FLUSH,
             sizeof(virtio_gpu_resource_flush_t),
             sizeof(virtio_gpu_ctrl_hdr_t));
}

int virtio_gpu_get_display_info(virtio_gpu_dev_t *dev, uint32_t *info_out) {
    send_cmd(dev, VIRTIO_GPU_CMD_GET_DISPLAY_INFO,
             sizeof(virtio_gpu_ctrl_hdr_t),
             24 + 16 * 24);

    virtio_gpu_resp_display_info_t *resp =
        (virtio_gpu_resp_display_info_t *)(uint64_t)dev->respbuf_phys;

    int active = 0;
    for (int i = 0; i < 16; i++) {
        uint32_t enabled = resp->modes[i].enabled;
        info_out[i * 3 + 0] = enabled;
        info_out[i * 3 + 1] = enabled ? resp->modes[i].rect.width  : 0;
        info_out[i * 3 + 2] = enabled ? resp->modes[i].rect.height : 0;
        if (enabled) active++;
    }
    return active;
}

void virtio_gpu_resource_unref(virtio_gpu_dev_t *dev, uint32_t id) {
    virtio_gpu_resource_unref_t *cmd =
        (virtio_gpu_resource_unref_t *)(uint64_t)dev->cmdbuf_phys;
    cmd->resource_id = id;
    cmd->padding = 0;
    send_cmd(dev, VIRTIO_GPU_CMD_RESOURCE_UNREF,
             sizeof(virtio_gpu_resource_unref_t),
             sizeof(virtio_gpu_ctrl_hdr_t));
}
