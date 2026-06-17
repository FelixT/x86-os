#ifndef PCI_H
#define PCI_H

#include "stdint.h"

struct process_t;

typedef struct pci_device_t {
   uint8_t bus, slot, func;
   uint16_t vendor, device;
} pci_device_t;

typedef struct dma_alloc_t {
   uint32_t addr, size;
} dma_alloc_t;

void pci_check_devices();
uint32_t pci_map_device(struct process_t *process, uint16_t vendor, uint16_t device_id);
pci_device_t *get_pci_devices(int *count);
pci_device_t *pci_find_device(uint16_t vendor, uint16_t device_id);
void pci_disable_device(pci_device_t *device); // clear MMIO decode + bus mastering
void dma_cleanup(struct process_t *process); // free tracked DMA + silence devices on task death

#endif