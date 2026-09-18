// https://wiki.osdev.org/ATA_PIO_Mode

#include <stdbool.h>
#include <stdint.h>

#include "memory.h"
#include "ata.h"
#include "io.h"
#include "windowmgr.h"

static uint32_t ata_base_lba = 0; // support partitions
static uint32_t ata_partition_sectors = 0; // 0 = whole disk, otherwise the partition size in sectors

bool ata_readwrite(bool primaryBus, bool masterDrive, uint32_t lba, uint16_t *buf, uint16_t sectors, bool write);

static bool ata_check_mbr(void) {
   uint16_t mbr[256];
   ata_base_lba = 0;
   if(!ata_readwrite(true, true, 0, mbr, 1, false)) return false; // mbr in first sector

   uint8_t *m = (uint8_t*)mbr;
   if(m[510] != 0x55 || m[511] != 0xAA) return false; // check mbr magic bytes

   bool any_entries = false;
   for(int i = 0; i < 4; i++) {
      uint8_t *e = m + 0x1BE + i*16; // 0x1BE is partition table entry
      if(e[4] == 0) continue; // empty slot
      any_entries = true;
      if(e[4] != 0x7F) continue; // partition type - uses reserved value 0x7F
      // note: boot0 is padded with zeros so this supports using the whole hd (boot0 must be < 446 bytes)
      debug_printf("Found MBR partition %i\n", i);
      uint32_t starting_lba = *(uint32_t*)(e + 0x08);
      uint32_t sectors = *(uint32_t*)(e + 0x0C);
      if(sectors == 0 || starting_lba + sectors > (1 << 28)) return false;
      ata_base_lba = starting_lba;
      ata_partition_sectors = sectors;
      return true;
   }
   return !any_entries; // use entire hd for mbr if partition table is empty
}

typedef struct {
   uint32_t d1;
   uint16_t d2, d3;
   uint8_t  d4[8];
} __attribute__((packed)) guid_t;

// guid partition table
// https://wiki.osdev.org/GPT
typedef struct {
   guid_t type;
   guid_t unique; // partition id
   uint64_t starting_lba;
   uint64_t ending_lba;
   uint64_t attributes;
   uint16_t name[36]; // UTF-16LE
} __attribute__((packed)) gpt_entry_t;

static const guid_t GPT_TYPE_GUID = { 0xBE59EB2E, 0x91C0, 0x4F56, {0x8F, 0xEF, 0x11, 0x9A, 0x41, 0xDA, 0x9F, 0xAA} };
static bool ata_check_gpt(void) {
   // read lba 1
   uint16_t sector[256];
   if(!ata_readwrite(true, true, 1, sector, 1, false)) return false;
   uint8_t *s = (uint8_t*)sector;
   if(memcmp(s, "EFI PART", 8) != 0) return false;

   uint32_t start_lba = *(uint32_t*)(s + 0x48);
   uint32_t count = *(uint32_t*)(s + 0x50);
   uint32_t entry_size = *(uint32_t*)(s + 0x54);

   if(entry_size < sizeof(gpt_entry_t) || 512 % entry_size != 0 || start_lba >= (1 << 28)) return false;
   if((int)count > 128) return false; // too many partitions, likely corrupt

   uint32_t per_sector = 512 / entry_size;
   for(int i = 0; i < (int)count; i++) {
      // read sectors of GPT
      if((i % per_sector) == 0) {
         if(!ata_readwrite(true, true, (uint32_t)start_lba + (i/per_sector), sector, 1, false))
            return false;
      }

      gpt_entry_t *entry = (gpt_entry_t*)((uint8_t*)sector+(i%per_sector)*entry_size);
      if(memcmp(entry, &GPT_TYPE_GUID, 16) != 0) continue;

      if(entry->starting_lba > entry->ending_lba) return false; // corrupt
      if(entry->ending_lba >= (1u << 28)) return false; // outside supported range
      ata_base_lba = entry->starting_lba;
      ata_partition_sectors = entry->ending_lba - entry->starting_lba + 1;
      return true;
   }
   return false;
}

void ata_find_partition(void) {
   if(ata_check_gpt()) {
      debug_printf("GPT HDD detected\n");
   } else if(ata_check_mbr()) {
      debug_printf("MBR HDD detected\n");
   } else {
      debug_printf("HDD isn't formatted to GPT or MBR, or system partition not found\n");
      while(true) {} // hang
   }
   
   debug_printf("ATA: base lba %u / partition sectors %u\n", ata_base_lba, ata_partition_sectors);
}

void ata_delay(uint16_t ioPort) {
   // create 400ns delay through 4 alternative status queries
   for(volatile int i = 0; i < 4; i++) {
      inb(ioPort + ATA_PORT_CONTROL_OFFSET + ATA_CTRL_REG_ALT_STATUS);
   }
}

// >= 5us required for SRST
static void ata_delay_long(uint16_t ioPort) {
   for(volatile int i = 0; i < ATA_RESET_DELAYS; i++)
      ata_delay(ioPort);
}

void ata_identify(bool primaryBus, bool masterDrive) {
   // select drive
   uint16_t ioPort;
   if(primaryBus) {
      ioPort = ATA_PORT_PRIMARY;
      if(masterDrive) outb(ioPort + ATA_REG_DRIVE_SELECT, ATA_SELECT_MASTER);
      else outb(ioPort + ATA_REG_DRIVE_SELECT, ATA_SELECT_SLAVE);
   } else {
      ioPort = ATA_PORT_SECONDARY;
      if(masterDrive) outb(ioPort + ATA_REG_DRIVE_SELECT, ATA_SELECT_MASTER);
      else outb(ioPort + ATA_REG_DRIVE_SELECT, ATA_SELECT_SLAVE);
   }
   // zero values before identify according to spec
   outb(ioPort + ATA_REG_SECTOR_COUNT, 0);
   outb(ioPort + ATA_REG_LBA0, 0);
   outb(ioPort + ATA_REG_LBA1, 0);
   outb(ioPort + ATA_REG_LBA2, 0);

   ata_delay(ioPort);

   // send identify cmd and read status
   outb(ioPort + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
   ata_delay(ioPort);
   uint8_t status = inb(ioPort + ATA_REG_STATUS);
   debug_printf("Status %i\n", status);

   if(status) {
      // poll until no longer busy
      while((status = inb(ioPort + ATA_REG_STATUS)) & ATA_STATUS_BIT_BSY);

      // poll until ready & DRQ is set, or until error
      while(!((status = inb(ioPort + ATA_REG_STATUS)) & ATA_STATUS_BIT_DRQ) && !(status & ATA_STATUS_BIT_ERR));
      if(status & ATA_STATUS_BIT_ERR) {
         gui_writestr("ATA DRIVE ERROR 1.", 0);
         return;
      }

      uint16_t buf[256];
      // read 256 words
      insw(ioPort + ATA_REG_DATA, buf, 256);

      uint32_t sectors = buf[60] + (buf[61]<<16); // hd size in 512 byte sectors
      debug_printf("Sectors: %u\n", sectors);

   } else {
      debug_printf("ATA DRIVE DOES NOT EXIST.\n", 0);
      // doesn't exist...
   }

   ata_find_partition();
}

// recover from failed rw - ensure next command doesn't read stale words
void ata_recover(uint16_t ioPort) {
   uint16_t ctrlPort = ioPort + ATA_PORT_CONTROL_OFFSET + ATA_CTRL_REG_DEV_CONTROL;

   // drain any data left over from the aborted transfer
   uint32_t spins = 0;
   while((inb(ioPort + ATA_REG_STATUS) & ATA_STATUS_BIT_DRQ) && spins++ < ATA_DRAIN_SPINS)
      inw(ioPort + ATA_REG_DATA);

   // soft reset
   outb(ctrlPort, ATA_CTRL_SRST);
   ata_delay_long(ioPort);
   outb(ctrlPort, 0);
   ata_delay_long(ioPort);

   // wait for the drive to come back up
   spins = 0;
   while((inb(ioPort + ATA_REG_STATUS) & ATA_STATUS_BIT_BSY) && spins++ < ATA_POLL_SPINS)
      asm volatile("pause" ::: "memory");
}

bool ata_readwrite(bool primaryBus, bool masterDrive, uint32_t lba, uint16_t *buf, uint16_t sectors, bool write) {
   // read or write n sectors (512 bytes) using ATA PIO mode

   if(ata_partition_sectors && lba + sectors > ata_partition_sectors) return false; // can't readwrite outside of partition size
   lba += ata_base_lba;

   if(sectors == 0 || sectors > ATA_MAX_SECTORS) {
      gui_printf("ATA: bad sector count %u\n", 0, sectors);
      return false;
   }

   uint16_t ioPort;
   
   if(primaryBus) ioPort = ATA_PORT_PRIMARY;
   else ioPort = ATA_PORT_SECONDARY;

   uint8_t initByte = (masterDrive ? 0xE0:0xF0);
   initByte |= (lba >> 24) & 0x0F;
   outb(ioPort + ATA_REG_DRIVE_SELECT, initByte);
   ata_delay(ioPort);
   outb(ioPort + ATA_REG_FEATURES, 0);
   outb(ioPort + ATA_REG_SECTOR_COUNT, (uint8_t)(sectors & 0xFF)); // count is 8 bit (0 = 256)
   outb(ioPort + ATA_REG_LBA0, (uint8_t)((lba)));
   outb(ioPort + ATA_REG_LBA1, (uint8_t)((lba)>>8));
   outb(ioPort + ATA_REG_LBA2, (uint8_t)((lba)>>16));

   uint8_t cmd = write ? ATA_CMD_WRITE_PIO : ATA_CMD_READ_PIO;
   outb(ioPort + ATA_REG_COMMAND, cmd);

   ata_delay(ioPort);
   
   uint8_t status;

   uint32_t spins;

   for(uint32_t i = 0; i < sectors; i++) {
      // poll until ready & DRQ is set, or until error
      spins = 0;
      do {
         status = inb(ioPort + ATA_REG_STATUS);
         asm volatile("pause" ::: "memory");
      } while((status & ATA_STATUS_BIT_BSY) && spins++ < ATA_POLL_SPINS);
      if(status & ATA_STATUS_BIT_BSY) {
         gui_printf("ATA: drive stuck busy, LBA 0x%h\n", 0, lba);
         ata_recover(ioPort);
         return false;
      }

      spins = 0;
      do {
         status = inb(ioPort + ATA_REG_STATUS);
         asm volatile("pause" ::: "memory");
      } while(!(status & ATA_STATUS_BIT_DRQ) && !(status & ATA_STATUS_BIT_ERR) && spins++ < ATA_POLL_SPINS);
      
      if(status & (ATA_STATUS_BIT_ERR | ATA_STATUS_BIT_DF)) {
         gui_writestr("ATA DRIVE ERROR 2.\n", 0);
         uint8_t error = inb(ioPort + ATA_REG_ERROR);
         gui_printf("LBA 0x%h\nError: 0x%h\n", 0, lba, error);
         ata_recover(ioPort);
         return false;
      }
      if(!(status & ATA_STATUS_BIT_DRQ)) {
         gui_printf("ATA: no data ready, LBA 0x%h\n", 0, lba);
         ata_recover(ioPort);
         return false;
      }

      // r/w 256 words
      uint32_t offset = i*256;
      if(write) {
         outsw(ioPort + ATA_REG_DATA, &buf[offset], 256);
      } else {
         insw(ioPort + ATA_REG_DATA, &buf[offset], 256);
      }

      ata_delay(ioPort);
   }

   if(write) {
      outb(ioPort + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
      ata_delay(ioPort);
      spins = 0;
      do {
         status = inb(ioPort + ATA_REG_STATUS);
         asm volatile("pause" ::: "memory");
      } while((status & ATA_STATUS_BIT_BSY) && spins++ < ATA_POLL_SPINS);

      if(status & (ATA_STATUS_BIT_BSY | ATA_STATUS_BIT_ERR | ATA_STATUS_BIT_DF)) {
         gui_printf("ATA: cache flush failed, LBA 0x%h\n", 0, lba);
         ata_recover(ioPort);
         return false;
      }
   }

   return true;
}

bool ata_read_exact_into(bool primaryBus, bool masterDrive, uint32_t addr, uint32_t bytes, uint8_t *outBuf) {
   if(!outBuf) return false;
   if(bytes == 0) return true;

   uint32_t startSector = addr/512;
   uint32_t offset = addr%512;
   uint32_t endAddr = addr + bytes;
   uint32_t endSector = (endAddr + (512-1)) / 512;
   uint32_t sectorCount = endSector - startSector;
   uint32_t bytesRequired = sectorCount*512;

   // read that fits within a single sector
   if(sectorCount == 1) {
      uint16_t sectorBuf[256]; // 512 bytes
      if(!ata_readwrite(primaryBus, masterDrive, startSector, sectorBuf, 1, false))
         return false;
      memcpy_fast(outBuf, ((uint8_t*)sectorBuf) + offset, bytes);
      return true;
   }

   // fast path for aligned read of entire sector
   if(offset == 0 && bytes % ATA_SECTOR_SIZE == 0 && bytes/ATA_SECTOR_SIZE <= ATA_MAX_SECTORS)
      return ata_readwrite(primaryBus, masterDrive, startSector, (uint16_t*)outBuf, (uint16_t)(bytes/ATA_SECTOR_SIZE), false);
   
   // read multiple sectors
   uint16_t *readBuf = malloc(bytesRequired);
   if(!readBuf) return false;
   
   // read all sectors at once if reasonable, or in chunks
   for(uint32_t i = 0; i < sectorCount; i += ATA_MAX_SECTORS) {
      uint32_t sectorsToRead = (i + ATA_MAX_SECTORS > sectorCount) ? (sectorCount - i) : ATA_MAX_SECTORS;
      if(!ata_readwrite(primaryBus, masterDrive, startSector + i, &readBuf[256*i], (uint16_t)sectorsToRead, false)) {
         free((uint32_t)readBuf, bytesRequired);
         return false;
      }
   }
   
   // copy only the required bytes
   memcpy_fast(outBuf, ((uint8_t*)readBuf) + offset, bytes);
   free((uint32_t)readBuf, bytesRequired);
   return true;
}

uint8_t *ata_read_exact(bool primaryBus, bool masterDrive, uint32_t addr, uint32_t bytes) {
   uint8_t *outBuf = malloc(bytes);
   if(!outBuf) return NULL;
   if(!ata_read_exact_into(primaryBus, masterDrive, addr, bytes, outBuf)) {
      free((uint32_t)outBuf, bytes);
      return NULL;
   }
   return outBuf;
}

bool ata_write_exact(bool primaryBus, bool masterDrive, uint32_t addr, uint8_t *buf, int size) {
   //debug_printf("Writing %u bytes to 0x%h\n", size, addr);
   if(size < 0) {
      debug_writestr("Write size must not be negative\n");
      return false;
   }
   if(size == 0) return true; // nothing to write
   if(size%ATA_SECTOR_SIZE != 0) {
      debug_writestr("Must read a multiple of 512 bytes\n");
      return false;
   }
   if(addr%ATA_SECTOR_SIZE != 0) {
      debug_writestr("Must read addr 512 bytes aligned\n");
      return false;
   }
   uint32_t startSector = addr/ATA_SECTOR_SIZE;
   uint32_t sectorCount = size/ATA_SECTOR_SIZE;
   for(uint32_t i = 0; i < sectorCount; i += ATA_MAX_SECTORS) {
      uint32_t sectorsToWrite = (i + ATA_MAX_SECTORS > sectorCount) ? (sectorCount - i) : ATA_MAX_SECTORS;
      if(!ata_readwrite(primaryBus, masterDrive, startSector + i, (uint16_t*)&buf[i*ATA_SECTOR_SIZE], (uint16_t)sectorsToWrite, true))
         return false;
   }
   return true;
}

void ata_interrupt(registers_t *regs) {
   (void)regs;
   // read status
   uint16_t ioPort;
   ioPort = ATA_PORT_PRIMARY;

   inb(ioPort + ATA_REG_STATUS);
}