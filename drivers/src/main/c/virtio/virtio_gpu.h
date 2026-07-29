#ifndef VIRTIO_GPU_H
#define VIRTIO_GPU_H

#include <stdint.h>

typedef struct virtio_gpu_dev virtio_gpu_dev_t;

#define VIRTIO_GPU_CMD_GET_DISPLAY_INFO     0x0100
#define VIRTIO_GPU_CMD_RESOURCE_CREATE_2D   0x0101
#define VIRTIO_GPU_CMD_RESOURCE_UNREF       0x0102
#define VIRTIO_GPU_CMD_SET_SCANOUT          0x0103
#define VIRTIO_GPU_CMD_RESOURCE_FLUSH       0x0104
#define VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D  0x0105
#define VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING 0x0106
#define VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING 0x0107
#define VIRTIO_GPU_CMD_GET_CAPSET_INFO      0x0108
#define VIRTIO_GPU_CMD_GET_CAPSET           0x0109
#define VIRTIO_GPU_CMD_GET_EDID             0x010A

#define VIRTIO_GPU_RESP_OK_NODATA           0x1100
#define VIRTIO_GPU_RESP_OK_DISPLAY_INFO     0x1101
#define VIRTIO_GPU_RESP_OK_CAPSET_INFO      0x1102
#define VIRTIO_GPU_RESP_OK_CAPSET           0x1103
#define VIRTIO_GPU_RESP_OK_EDID             0x1104

#define VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM    1
#define VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM    2
#define VIRTIO_GPU_FORMAT_R8G8B8X8_UNORM    3
#define VIRTIO_GPU_FORMAT_R8G8B8A8_UNORM    4

typedef struct {
    uint32_t type;
    uint32_t flags;
    uint64_t fence_id;
    uint32_t ctx_id;
    uint32_t padding;
} __attribute__((packed)) virtio_gpu_ctrl_hdr_t;

typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t resource_id;
    uint32_t format;
    uint32_t width;
    uint32_t height;
} __attribute__((packed)) virtio_gpu_resource_create_2d_t;

typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t resource_id;
    uint32_t nr_entries;
    struct {
        uint64_t addr;
        uint32_t length;
        uint32_t padding;
    } entries[];
} __attribute__((packed)) virtio_gpu_resource_attach_backing_t;

typedef struct {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} __attribute__((packed)) virtio_gpu_rect_t;

typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_rect_t     r;
    uint32_t              scanout_id;
    uint32_t              resource_id;
} __attribute__((packed)) virtio_gpu_set_scanout_t;

typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_rect_t     r;
    uint64_t              offset;
    uint32_t              resource_id;
    uint32_t              padding;
} __attribute__((packed)) virtio_gpu_transfer_to_host_2d_t;

typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_rect_t     r;
    uint32_t              resource_id;
    uint32_t              padding;
} __attribute__((packed)) virtio_gpu_resource_flush_t;

typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t resource_id;
    uint32_t padding;
} __attribute__((packed)) virtio_gpu_resource_unref_t;

typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
} __attribute__((packed)) virtio_gpu_get_display_info_t;

typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    struct {
        struct {
            uint32_t x;
            uint32_t y;
            uint32_t width;
            uint32_t height;
        } rect;
        uint32_t enabled;
        uint32_t flags;
    } modes[16];
} __attribute__((packed)) virtio_gpu_resp_display_info_t;

int virtio_gpu_dev_size(void);
void virtio_gpu_dev_init(virtio_gpu_dev_t *dev, uint64_t common_phys,
                         uint64_t notify_phys, uint64_t device_phys,
                         uint64_t cmdbuf, uint64_t respbuf);
void virtio_gpu_create_2d(virtio_gpu_dev_t *dev, uint32_t id, uint32_t format,
                          uint32_t width, uint32_t height);
void virtio_gpu_attach_backing(virtio_gpu_dev_t *dev, uint32_t id,
                               uint64_t phys, uint32_t size);
void virtio_gpu_set_scanout(virtio_gpu_dev_t *dev, uint32_t scanout,
                            uint32_t id, uint32_t width, uint32_t height);
void virtio_gpu_transfer_to_host_2d(virtio_gpu_dev_t *dev, uint32_t id,
                                    uint32_t x, uint32_t y,
                                    uint32_t width, uint32_t height);
void virtio_gpu_flush(virtio_gpu_dev_t *dev, uint32_t id,
                      uint32_t x, uint32_t y, uint32_t width, uint32_t height);
int virtio_gpu_get_display_info(virtio_gpu_dev_t *dev, uint32_t *info_out);
void virtio_gpu_resource_unref(virtio_gpu_dev_t *dev, uint32_t id);

#endif
