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

// map device
bool pci_map(kobj_inst_t *inst, int index, kobj_mapping_t *mapping) {
   if(index != 1) return false; // currently only supports BAR1
   pci_device_t *device = inst->data;

   pci_device_bar_t *bar = &device->bars[index];
   mapping->paddr = bar->bar;
   mapping->size = bar->size;
   mapping->no_cache = 1;
   return true;
}

kobj_handle_t pci_connect(process_t *process, uint16_t vendor, uint16_t device_id) {
   pci_device_t *device = pci_find_device(vendor, device_id);
   if(!device) {
      debug_printf("PCI connect failed: couldn't find device to map\n");
      return -1;
   }
   if(device->inst) {
      debug_printf("PCI connect failed: device is already claimed\n");
      return -1;
   }

   for(int i = 0; i < 6; i++) {
      device->bars[i].bar = 0;
      device->bars[i].size = 0;
   }

   // get BAR1
   pci_device_bar_t *bar = &device->bars[1];
   bar->bar = pci_config_read32(device->bus, device->slot, device->func, 0x14) & ~0xF; // clear flag bits
   
   // read size
   // disable memory-space decode before probing
   uint32_t cmd = pci_config_read32(device->bus, device->slot, device->func, 0x04);
   pci_config_write32(device->bus, device->slot, device->func, 0x04, cmd & ~PCI_CMD_MMIO);

   pci_config_write32(device->bus, device->slot, device->func, 0x14, 0xFFFFFFFF); // write all-ones
   uint32_t probe_size = pci_config_read32(device->bus, device->slot, device->func, 0x14); // device returns size mask
   pci_config_write32(device->bus, device->slot, device->func, 0x14, bar->bar); // restore

   uint32_t size = ~(probe_size & ~0xF) + 1;
   if(size == 0) {
      pci_config_write32(device->bus, device->slot, device->func, 0x04, cmd); // restore
      return -1;
   }
   bar->size = size;

   // create kobj
   kobj_inst_t *inst = create_kobj_inst(KOBJ_PCI, device);
   if(!inst) {
      pci_config_write32(device->bus, device->slot, device->func, 0x04, cmd); // restore
      return -1;
   }

   kobj_handle_t h = process_acquire_kobj(process, inst);
   if(h < 0) {
      pci_config_write32(device->bus, device->slot, device->func, 0x04, cmd); // restore
      inst->stopped = true; // never started
      free_kobj_inst(inst);
      return -1;
   }
   device->inst = inst;
   
   // enable device - enable MMIO + ensure device starts with DMA disabled
   cmd = (cmd | PCI_CMD_MMIO) & ~PCI_CMD_DMA;
   pci_config_write32(device->bus, device->slot, device->func, 0x04, cmd);

   return h;
}

bool pci_control(kobj_inst_t *inst, int operation, uint32_t arg, uint32_t *out) {
   (void)out;
   pci_device_t *device = inst->data;

   switch(operation) {
      case PCI_CTRL_DMA:
         if(inst->stopped) return false;
         uint32_t cmd = pci_config_read32(device->bus, device->slot, device->func, 0x04);
         cmd = arg ? (cmd | PCI_CMD_DMA) : (cmd & ~PCI_CMD_DMA);
         pci_config_write32(device->bus, device->slot, device->func, 0x04, cmd);
         return true;
   }

   return false;
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

pci_device_t *get_pci_devices(int *count) {
   *count = pci_device_count;
   return &pci_devices[0];
}
