#include "pci.h"

#include "windowmgr.h"
#include "memory.h"
#include "paging.h"

// pci driver using config access mechanism #1

#define CONFIG_ADDRESS 0xCF8
#define CONFIG_DATA 0xCFC

#define PCI_BUS_COUNT 256
#define PCI_SLOT_COUNT 32
#define PCI_FUNC_COUNT 8

#define PCI_MAX_DEVICES 32
pci_device_t pci_devices[PCI_MAX_DEVICES];
int pci_device_count = 0;

uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
   uint32_t addr = (1u << 31) | (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC); // aligned
   outl(CONFIG_ADDRESS, addr);
   return inl(CONFIG_DATA);
}

void pci_config_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
   uint32_t addr = (1u << 31) | (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC);
   outl(CONFIG_ADDRESS, addr);
   outl(CONFIG_DATA, value);
}

// refresh pci_devices at boot
void pci_check_devices() {
   pci_device_count = 0;
   for(uint16_t bus = 0; bus < PCI_BUS_COUNT; bus++) {
      for(uint8_t slot = 0; slot < PCI_SLOT_COUNT; slot++) {
         for(uint8_t func = 0; func < PCI_FUNC_COUNT; func++) {
            if(pci_device_count == PCI_MAX_DEVICES) return;
            uint32_t id = pci_config_read32(bus, slot, func, 0x00);
            uint16_t vendor = id & 0xFFFF;
            if(vendor == 0xFFFF) break;
            pci_device_t *device = &pci_devices[pci_device_count];
            uint16_t device_id = id >> 16;
            device->bus = bus;
            device->slot = slot;
            device->func = func;
            device->vendor = vendor;
            device->device = device_id;
            device->inst = NULL;
            uint32_t dw3 = pci_config_read32(bus, slot, func, 0x0C);
            uint8_t header_type = (dw3 >> 16) & 0xFF;
            pci_device_count++;
            if(func == 0 && !(header_type & 0x80)) break; // single-function device
         }
      }
   }
}

pci_device_t *pci_find_device(uint16_t vendor, uint16_t device_id) {
   for(int i = 0; i < pci_device_count; i++) {
      pci_device_t *device = &pci_devices[i];
      if(vendor == device->vendor && device_id == device->device)
         return device;
   }
   return NULL;
}

#define PCI_CMD_MMIO (1 << 1)
#define PCI_CMD_DMA (1 << 2) // bus mastering

kobj_handle_t pci_map_device(process_t *process, uint16_t vendor, uint16_t device_id, uint32_t *vaddr) {
   // setup
   pci_device_t *device = pci_find_device(vendor, device_id);
   if(!device) {
      debug_printf("PCI map failed: couldn't find device to map\n");
      return -1;
   }
   if(device->inst) {
      debug_printf("PCI map failed: device is already mapped\n");
      return -1;
   }

   // get bar1 - todo: get arbitrary bar to support hardware beyond rtl
   uint32_t bar1 = pci_config_read32(device->bus, device->slot, device->func, 0x14) & ~0xF; // clear flag bits

   // read size
   // disable memory-space decode before probing
   uint32_t cmd = pci_config_read32(device->bus, device->slot, device->func, 0x04);
   pci_config_write32(device->bus, device->slot, device->func, 0x04, cmd & ~PCI_CMD_MMIO);

   pci_config_write32(device->bus, device->slot, device->func, 0x14, 0xFFFFFFFF); // write all-ones
   uint32_t probe_size = pci_config_read32(device->bus, device->slot, device->func, 0x14); // device returns size mask
   pci_config_write32(device->bus, device->slot, device->func, 0x14, bar1); // restore

   uint32_t size = ~(probe_size & ~0xF) + 1;
   if(size == 0) return -1;

   uint32_t size_total = ((size + (MEM_BLOCK_SIZE - 1)) / MEM_BLOCK_SIZE) * MEM_BLOCK_SIZE; // round up
   debug_printf("Mapping 0x%h size 0x%h (0x%h)\n", bar1, size, size_total);

   if(process->mmio_end + size_total > V_MMIO_END) {
      debug_printf("PCI map failed: No more vmem");
      return -1;
   }

   // create kobj
   kobj_inst_t *inst = create_kobj_inst(KOBJ_PCI, device);
   if(!inst)
      return -1;
   kobj_handle_t h = process_acquire_kobj(process, inst);
   if(h < 0) {
      inst->stopped = true; // never started
      free_kobj_inst(inst);
      return -1;
   }
   
   device->inst = inst;
   device->vaddr = process->mmio_end;
   process->mmio_end += size_total;
   device->size = size;

   inst->data = device;

   // map bar1 into task
   map_size(process->page_dir, bar1, device->vaddr, size, 1, 1, 1); // no cache

   // MMIO + DMA
   cmd |= PCI_CMD_MMIO | PCI_CMD_DMA;
   pci_config_write32(device->bus, device->slot, device->func, 0x04, cmd);

   *vaddr = device->vaddr;
   return h;
}

// clear mmio and dma enable flags
void pci_disable_device(pci_device_t *device) {
   uint32_t cmd = pci_config_read32(device->bus, device->slot, device->func, 0x04);
   cmd &= ~(PCI_CMD_MMIO | PCI_CMD_DMA);
   pci_config_write32(device->bus, device->slot, device->func, 0x04, cmd);
}

void pci_stop(kobj_inst_t *inst) {
   pci_disable_device(inst->data);
}

void pci_free_inst(kobj_inst_t *inst) {
   pci_device_t *device = inst->data;
   device->inst = NULL;
}

void pci_close(kobj_ref_t *ref, process_t *process, bool ending) {
   pci_device_t *device = ref->inst->data;
   if(!ending) {
      // unmap
      unmap_size(process->page_dir, device->vaddr, device->size);
   }
}

pci_device_t *get_pci_devices(int *count) {
   *count = pci_device_count;
   return &pci_devices[0];
}