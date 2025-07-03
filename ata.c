// https://wiki.osdev.org/ATA_PIO_Mode

#include <stdbool.h>
#include <stdint.h>

#include "memory.h"
#include "ata.h"
#include "io.h"
#include "windowmgr.h"

void ata_delay(uint16_t ioPort) {
   // create 400ns delay through 4 alternative status queries
   for(volatile int i = 0; i < 4; i++) {
      inb(ioPort + ATA_PORT_CONTROL_OFFSET + ATA_CTRL_REG_ALT_STATUS);
   }
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
   uint8_t status = inb(ioPort + ATA_REG_STATUS);
   gui_writenum(status, 0);

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
      for(int i = 0; i < 256; i++) {
         buf[i] = inw(ioPort + ATA_REG_DATA);
      }

      gui_writestr(": ", 0);
      uint32_t sectors = buf[60] + (buf[61]<<16); // hd size in 512 byte sectors
      gui_writeuint(sectors, 0);
      //uint32_t size = ()
      //gui_writenum(buf[])

   } else {
      gui_writestr("ATA DRIVE DOES NOT EXIST.", 0);
      // doesn't exist...
   }

}

void ata_readwrite(bool primaryBus, bool masterDrive, uint32_t lba, uint16_t *buf, bool write) {
   // read or write 512 bytes using ATA PIO mode

   uint16_t ioPort;
   
   if(primaryBus) ioPort = ATA_PORT_PRIMARY;
   else ioPort = ATA_PORT_SECONDARY;

   uint8_t initByte = (masterDrive ? 0xE0:0xF0);
   initByte |= (lba >> 24) & 0x0F;
   outb(ioPort + ATA_REG_DRIVE_SELECT, initByte);
   ata_delay(ioPort);
   outb(ioPort + ATA_REG_FEATURES, 0);
   outb(ioPort + ATA_REG_SECTOR_COUNT, 1);

   outb(ioPort + ATA_REG_LBA0, (uint8_t)((lba)));
   outb(ioPort + ATA_REG_LBA1, (uint8_t)((lba)>>8));
   outb(ioPort + ATA_REG_LBA2, (uint8_t)((lba)>>16));

   uint8_t cmd = write ? ATA_CMD_WRITE_PIO : ATA_CMD_READ_PIO;
   outb(ioPort + ATA_REG_COMMAND, cmd);

   ata_delay(ioPort);
   
   // poll until ready & DRQ is set, or until error
   volatile uint8_t status;
   do {
      status = inb(ioPort + ATA_REG_STATUS);
      asm volatile("pause" ::: "memory");
   } while(status & ATA_STATUS_BIT_BSY);
   do {
      status = inb(ioPort + ATA_REG_STATUS);
      asm volatile("pause" ::: "memory");
   } while(!(status & ATA_STATUS_BIT_DRQ) && !(status & ATA_STATUS_BIT_ERR));
   
   if(status & ATA_STATUS_BIT_ERR) {
      gui_writestr("ATA DRIVE ERROR 2.\n", 0);
      uint8_t error = inb(ioPort + ATA_REG_ERROR);
      gui_printf("LBA 0x%h\nError: 0x%h\n", 0, lba, error);
      while(true){};
      return;
   }

   // read 256 words
   for(int i = 0; i < 256; i++) {
      if(write) {
         outw(ioPort + ATA_REG_DATA, buf[i]);
      } else {
         buf[i] = inw(ioPort + ATA_REG_DATA);
      }
   }

   if(write) {
      outb(ioPort + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
      do {
         status = inb(ioPort + ATA_REG_STATUS);
         asm volatile("pause" ::: "memory");
      } while(status & ATA_STATUS_BIT_BSY);
   }
}

uint8_t *ata_read_exact(bool primaryBus, bool masterDrive, uint32_t addr, uint32_t bytes) {
   uint32_t startSector = addr/512;
   uint32_t offset = addr%512;
   uint32_t endAddr = addr + bytes;
   uint32_t endSector = (endAddr + (512-1)) / 512;
   uint32_t sectorCount = endSector - startSector;
   uint32_t bytesRequired = sectorCount*512;

   uint8_t *outBuf = malloc(bytes);
   if(!outBuf) return NULL;
   
   // read that fits within a single sector
   if(sectorCount == 1) {
      uint16_t sectorBuf[256]; // 512 bytes
      ata_readwrite(primaryBus, masterDrive, startSector, sectorBuf, false);
      memcpy_fast(outBuf, ((uint8_t*)sectorBuf) + offset, bytes);
      return outBuf;
   }
   
   // read multiple sectors
   uint16_t *readBuf = malloc(sectorCount*512);
   if(!readBuf) {
      free((uint32_t)outBuf, bytes);
      return NULL;
   }
   
   // read all sectors at once if reasonable, or in chunks
   const uint32_t MAX_SECTORS_PER_READ = 128; // safe value
   for(uint32_t i = 0; i < sectorCount; i += MAX_SECTORS_PER_READ) {
      uint32_t sectorsToRead = (i + MAX_SECTORS_PER_READ > sectorCount) ? (sectorCount - i) : MAX_SECTORS_PER_READ;
      for(uint32_t j = 0; j < sectorsToRead; j++)
         ata_readwrite(primaryBus, masterDrive, startSector + i + j, &readBuf[256*(i + j)], false);
   }
   
   // copy only the required bytes
   memcpy_fast(outBuf, ((uint8_t*)readBuf) + offset, bytes);
   free((uint32_t)&readBuf[0], bytesRequired);
   return outBuf;
}

void ata_write_exact(bool primaryBus, bool masterDrive, uint32_t addr, uint8_t *buf, int size) {
   //debug_printf("Writing %u bytes to 0x%h\n", size, addr);
   if(size%512 != 0) {
      debug_writestr("Must read a multiple of 512 bytes\n");
      return;
   }
   if(addr%512 != 0) {
      debug_writestr("Must read addr 512 bytes aligned\n");
      return;
   }
   for(int i = 0; i < size/512; i++)
      ata_readwrite(primaryBus, masterDrive, addr/512 + i, (uint16_t*)(buf + i*512), true);
}

void ata_interrupt() {
   // read status
   uint16_t ioPort;
   ioPort = ATA_PORT_PRIMARY;

   inb(ioPort + ATA_REG_STATUS);
}