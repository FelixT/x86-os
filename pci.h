#ifndef PCI_H
#define PCI_H

#include <stdint.h>
#include "kobj.h"

typedef struct process_t process_t;

typedef struct pci_device_t {
   uint8_t bus, slot, func;
   uint16_t vendor, device;
   kobj_inst_t *inst; // may be referenced by one kobj
   uint32_t vaddr;
   uint32_t size;
} pci_device_t;

void pci_check_devices();
kobj_handle_t pci_map_device(process_t *process, uint16_t vendor, uint16_t device_id, uint32_t *vaddr);
pci_device_t *get_pci_devices(int *count);
pci_device_t *pci_find_device(uint16_t vendor, uint16_t device_id);
void pci_stop(kobj_inst_t *inst); // clear MMIO decode + bus mastering
void pci_free_inst(kobj_inst_t *inst);
void pci_close(kobj_ref_t *ref, process_t *process, bool ending);

#endif