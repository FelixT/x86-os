// https://wiki.osdev.org/ATA_PIO_Mode

#include <stdbool.h>
#include <stdint.h>

#include "memory.h"
#include "ata.h"
#include "io.h"

void ata_delay(uint16_t ioPort) {
   // create 400ns delay through 4 alternative status queries
   for(int i = 0; i < 4; i++) {
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

void ata_read(bool primaryBus, bool masterDrive, uint32_t lba, uint16_t *buf) {
   // read 512 bytes using ATA PIO mode

   uint16_t ioPort;
   
   if(primaryBus) ioPort = ATA_PORT_PRIMARY;
   else ioPort = ATA_PORT_SECONDARY;

   uint8_t initByte = (masterDrive ? 0xE0:0xF0);
   initByte |= (lba << 24) & 0x0F;
   outb(ioPort + ATA_REG_DRIVE_SELECT, initByte);
   outb(ioPort + ATA_REG_FEATURES, 0);

   outb(ioPort + ATA_REG_SECTOR_COUNT, 1);

   outb(ioPort + ATA_REG_LBA0, (uint8_t)((lba)));
   outb(ioPort + ATA_REG_LBA1, (uint8_t)((lba)>>8));
   outb(ioPort + ATA_REG_LBA2, (uint8_t)((lba)>>16));

   outb(ioPort + ATA_REG_COMMAND, ATA_CMD_READ_PIO);

   ata_delay(ioPort);
   uint8_t status = inb(ioPort + ATA_REG_STATUS);
   // poll until no longer busy
   while((status = inb(ioPort + ATA_REG_STATUS)) & ATA_STATUS_BIT_BSY);

   // poll until ready & DRQ is set, or until error
   while(!((status = inb(ioPort + ATA_REG_STATUS)) & ATA_STATUS_BIT_DRQ) && !(status & ATA_STATUS_BIT_ERR));
   if(status & ATA_STATUS_BIT_ERR) {
      gui_writestr("ATA DRIVE ERROR 2.", 0);
      return;
   }

   // read 256 words
   for(int i = 0; i < 256; i++) {
      buf[i] = inw(ioPort + ATA_REG_DATA);
   }

}

uint8_t *ata_read_exact(bool primaryBus, bool masterDrive, uint32_t addr, uint32_t bytes) {
   uint16_t ioPort;
   if(primaryBus) ioPort = ATA_PORT_PRIMARY;
   else ioPort = ATA_PORT_SECONDARY;

   uint32_t lba = addr/512;
   uint32_t startAddr = lba*512;

   uint32_t offset = addr - startAddr;
   uint32_t endAddr = startAddr + offset + bytes; // = addr+bytes
   //uint32_t extraBytes = 512 - offset;
   uint32_t diffAddr = endAddr - startAddr;
   int reads = (diffAddr + (512-1))/512;
   uint32_t bytesRequired = reads*512;

   uint16_t *readBuf = malloc(bytesRequired);
   

   //int reads = (bytesRequired + (512-1))/512; // bytes
   for(int i = 0; i < reads; i++) {
      ata_read(primaryBus, masterDrive, lba+i, &readBuf[256*i]);
      //ata_delay(ioPort);
   }

   // copy to new buffer
   uint8_t *outBuf = malloc(bytes);
   for(uint32_t i = 0; i < bytes; i++) {
      outBuf[i] = *((uint8_t *) (&readBuf[0]) + (offset + i));
   }

   free((uint32_t)&readBuf[0], bytesRequired);
   return outBuf;
}

void ata_interrupt() {
   // read status
   uint16_t ioPort;
   ioPort = ATA_PORT_PRIMARY;

   inb(ioPort + ATA_REG_STATUS);
}