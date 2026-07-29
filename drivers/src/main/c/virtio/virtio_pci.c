#include "virtio_pci.h"
#include <stdint.h>

void virtio_pci_select_queue(void *common_cfg, uint16_t idx) {
    volatile virtio_pci_common_cfg_t *cfg = (volatile virtio_pci_common_cfg_t *)common_cfg;
    cfg->queue_select = idx;
}

void virtio_pci_set_queue(void *common_cfg,
                          uint64_t desc, uint64_t driver, uint64_t device) {
    volatile virtio_pci_common_cfg_t *cfg = (volatile virtio_pci_common_cfg_t *)common_cfg;
    volatile uint32_t *p = (volatile uint32_t *)cfg;
    p[0x20 / 4] = (uint32_t)desc;
    p[0x24 / 4] = (uint32_t)(desc >> 32);
    p[0x28 / 4] = (uint32_t)driver;
    p[0x2C / 4] = (uint32_t)(driver >> 32);
    p[0x30 / 4] = (uint32_t)device;
    p[0x34 / 4] = (uint32_t)(device >> 32);
}

void virtio_pci_enable_queue(void *common_cfg) {
    volatile virtio_pci_common_cfg_t *cfg = (volatile virtio_pci_common_cfg_t *)common_cfg;
    cfg->queue_enable = 1;
}

uint16_t virtio_pci_get_queue_size(void *common_cfg) {
    volatile virtio_pci_common_cfg_t *cfg = (volatile virtio_pci_common_cfg_t *)common_cfg;
    return cfg->queue_size;
}

void virtio_pci_set_status(void *common_cfg, uint8_t status) {
    volatile virtio_pci_common_cfg_t *cfg = (volatile virtio_pci_common_cfg_t *)common_cfg;
    cfg->device_status = status;
}

uint8_t virtio_pci_get_status(void *common_cfg) {
    volatile virtio_pci_common_cfg_t *cfg = (volatile virtio_pci_common_cfg_t *)common_cfg;
    return cfg->device_status;
}

uint32_t virtio_pci_get_features(void *common_cfg) {
    volatile virtio_pci_common_cfg_t *cfg = (volatile virtio_pci_common_cfg_t *)common_cfg;
    cfg->device_feature_select = 0;
    return cfg->device_feature;
}

void virtio_pci_set_features(void *common_cfg, uint32_t features) {
    volatile virtio_pci_common_cfg_t *cfg = (volatile virtio_pci_common_cfg_t *)common_cfg;
    cfg->driver_feature_select = 0;
    cfg->driver_feature = features;
}

void virtio_pci_disable_interrupts(void *common_cfg) {
    volatile virtio_pci_common_cfg_t *cfg = (volatile virtio_pci_common_cfg_t *)common_cfg;
    cfg->queue_msix_vector = 0xFFFF;
}

uint8_t virtio_pci_isr_status(void *isr) {
    return *(volatile uint8_t *)isr;
}

void virtio_pci_notify(void *notify, uint16_t queue_notify_off) {
    volatile uint16_t *n = (volatile uint16_t *)((uint8_t *)notify + queue_notify_off);
    *n = 0;
}
