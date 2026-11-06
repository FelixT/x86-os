#ifndef PCI_H
#define PCI_H

#include <stdint.h>
#include "kobj.h"

typedef struct process_t process_t;

typedef struct pci_device_bar_t {
   uint32_t bar;
   int size;
} pci_device_bar_t;

typedef struct pci_device_t {
   uint8_t bus, slot, func;
   uint16_t vendor, device;
   kobj_inst_t *inst; // may be referenced by one kobj
   pci_device_bar_t bars[6];
} pci_device_t;

void pci_check_devices();
kobj_handle_t pci_connect(process_t *process, uint16_t vendor, uint16_t device_id);
bool pci_map(kobj_inst_t *inst, int index, kobj_mapping_t *mapping);
pci_device_t *get_pci_devices(int *count);
pci_device_t *pci_find_device(uint16_t vendor, uint16_t device_id);
void pci_stop(kobj_inst_t *inst); // clear MMIO decode + bus mastering
void pci_free_inst(kobj_inst_t *inst);
bool pci_control(kobj_inst_t *inst, int operation, uint32_t arg, uint32_t *out);

#endif