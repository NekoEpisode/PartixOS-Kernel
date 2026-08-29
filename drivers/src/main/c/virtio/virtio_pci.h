#ifndef VIRTIO_PCI_H
#define VIRTIO_PCI_H

#include <stdint.h>

#define VIRTIO_PCI_CAP_COMMON_CFG  1
#define VIRTIO_PCI_CAP_NOTIFY_CFG  2
#define VIRTIO_PCI_CAP_ISR_CFG     3
#define VIRTIO_PCI_CAP_DEVICE_CFG  4
#define VIRTIO_PCI_CAP_PCI_CFG     5

#define VIRTIO_PCI_VENDOR_ID       0x1AF4
#define VIRTIO_PCI_DEVICE_ID_GPU   0x1050

typedef struct {
    uint8_t  cap_vndr;
    uint8_t  cap_next;
    uint8_t  cfg_type;
    uint8_t  bar;
    uint8_t  id;
    uint8_t  padding[2];
    uint32_t offset;
    uint32_t length;
} __attribute__((packed)) virtio_pci_cap_t;

typedef struct {
    uint32_t device_feature_select;
    uint32_t device_feature;
    uint32_t driver_feature_select;
    uint32_t driver_feature;
    uint16_t msix_config;
    uint16_t num_queues;
    uint8_t  device_status;
    uint8_t  config_generation;
    uint16_t queue_select;
    uint16_t queue_size;
    uint16_t queue_msix_vector;
    uint16_t queue_enable;
    uint16_t queue_notify_off;
    uint64_t queue_desc;
    uint64_t queue_driver;
    uint64_t queue_device;
} __attribute__((packed)) virtio_pci_common_cfg_t;

void virtio_pci_select_queue(void *common_cfg, uint16_t idx);
void virtio_pci_set_queue(void *common_cfg, uint64_t desc, uint64_t driver, uint64_t device);
void virtio_pci_enable_queue(void *common_cfg);
uint16_t virtio_pci_get_queue_size(void *common_cfg);
void virtio_pci_set_status(void *common_cfg, uint8_t status);
uint8_t virtio_pci_get_status(void *common_cfg);
uint32_t virtio_pci_get_features(void *common_cfg);
void virtio_pci_set_features(void *common_cfg, uint32_t features);
void virtio_pci_disable_interrupts(void *common_cfg);
uint16_t virtio_pci_get_queue_notify_off(void *common_cfg);

#endif
