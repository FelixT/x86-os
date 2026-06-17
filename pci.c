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

// refresh pci_devices
void pci_check_devices() {
   pci_device_count = 0;
   for(uint16_t bus = 0; bus < PCI_BUS_COUNT; bus++) {
      for(uint8_t slot = 0; slot < PCI_SLOT_COUNT; slot++) {
         for(uint8_t func = 0; func < PCI_FUNC_COUNT; func++) {
            if(pci_device_count == PCI_MAX_DEVICES) return;
            uint32_t id = pci_config_read32(bus, slot, func, 0x00);
            uint16_t vendor = id & 0xFFFF;
            if(vendor == 0xFFFF) break;
            uint16_t device_id = id >> 16;
            pci_devices[pci_device_count].bus = bus;
            pci_devices[pci_device_count].slot = slot;
            pci_devices[pci_device_count].func = func;
            pci_devices[pci_device_count].vendor = vendor;
            pci_devices[pci_device_count].device = device_id;
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

uint32_t pci_map_device(process_t *process, uint16_t vendor, uint16_t device_id) {
   // setup
   pci_device_t *device = pci_find_device(vendor, device_id);
   if(!device) {
      debug_printf("PCI map failed: couldn't find device to map\n");
      return 0;
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
   if(size == 0) return 0;

   // MMIO + DMA
   cmd |= PCI_CMD_MMIO | PCI_CMD_DMA;
   pci_config_write32(device->bus, device->slot, device->func, 0x04, cmd);

   uint32_t size_total = ((size + (MEM_BLOCK_SIZE - 1)) / MEM_BLOCK_SIZE) * MEM_BLOCK_SIZE; // round up
   debug_printf("Mapping 0x%h size 0x%h (0x%h)\n", bar1, size, size_total);

   if(process->mmio_end + size_total > V_MMIO_END) {
      debug_printf("PCI map failed: No more vmem");
      return 0;
   }
   uint32_t addr = process->mmio_end;
   // map bar1 into task
   map_size(process->page_dir, bar1, process->mmio_end, size, 1, 1, 1); // no cache
   process->mmio_end += size_total;

   // track so we can silence bus mastering if the driver dies (see dma_cleanup)
   bool tracked = false;
   for(int i = 0; i < process->device_count; i++)
      if(process->devices[i] == device) { tracked = true; break; }
   if(!tracked && process->device_count < TASK_MAX_PCI)
      process->devices[process->device_count++] = device;

   return addr;
}

// unmap - clear mmio and dma enable flags
void pci_disable_device(pci_device_t *device) {
   uint32_t cmd = pci_config_read32(device->bus, device->slot, device->func, 0x04);
   cmd &= ~(PCI_CMD_MMIO | PCI_CMD_DMA);
   pci_config_write32(device->bus, device->slot, device->func, 0x04, cmd);
}

// stop all pci devices and reclaim dma memory
void dma_cleanup(process_t *process) {
   for(int i = 0; i < process->device_count; i++)
      pci_disable_device(process->devices[i]);
   process->device_count = 0;

   for(int i = 0; i < process->dma_count; i++)
      free(process->dma_allocs[i].addr, process->dma_allocs[i].size);
   process->dma_count = 0;
}

pci_device_t *get_pci_devices(int *count) {
   *count = pci_device_count;
   return &pci_devices[0];
}