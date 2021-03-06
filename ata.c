// https://wiki.osdev.org/ATA_PIO_Mode

#include <stdbool.h>
#include <stdint.h>

#include "memory.h"

#define ATA_CMD_IDENTIFY 0xEC
#define ATA_CMD_READ_PIO 0x20

#define ATA_SELECT_MASTER 0xA0
#define ATA_SELECT_SLAVE 0xB0

// io bases
#define ATA_PORT_PRIMARY 0x1F0
#define ATA_PORT_SECONDARY 0x170

// control bases
#define ATA_PORT_CONTROL_OFFSET 0x206

// registers
#define ATA_REG_DATA 0x00 // read&write pio data bytes
#define ATA_REG_ERROR 0x01
#define ATA_REG_FEATURES 0x01
#define ATA_REG_SECTOR_COUNT 0x02
#define ATA_REG_LBA0 0x03
#define ATA_REG_LBA1 0x04
#define ATA_REG_LBA2 0x05
#define ATA_REG_DRIVE_SELECT 0x06 // or head select
#define ATA_REG_STATUS 0x07
#define ATA_REG_COMMAND 0x07

#define ATA_CTRL_REG_ALT_STATUS 0x00

#define ATA_STATUS_BIT_ERR 0x01
#define ATA_STATUS_BIT_DRQ 0x08
#define ATA_STATUS_BIT_SRV 0x10
#define ATA_STATUS_BIT_DF 0x20
#define ATA_STATUS_BIT_RDY 0x40
#define ATA_STATUS_BIT_BSY 0x80

#define ATA_SECTOR_SIZE 512 // bytes

static inline uint8_t inb(uint16_t port) {
   uint8_t ret;
   asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
   return ret;
}

static inline uint16_t inw(uint16_t port) {
   uint16_t ret;
   asm volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
   return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
   asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

void ata_delay(uint16_t ioPort) {
   // create 400ns delay through 4 alternative status queries
   for(int i = 0; i < 4; i++) {
      inb(ioPort + ATA_PORT_CONTROL_OFFSET + ATA_CTRL_REG_ALT_STATUS);
   }
}

extern void gui_writenum(int num, int colour);
extern void gui_writeuint(uint32_t num, int colour);
extern void gui_writestr(char *c, int colour);
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
         gui_writestr("ATA DRIVE ERROR.", 0);
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
   // read using ATA PIO mode

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
   outb(ioPort + ATA_REG_LBA1, (uint8_t)((lba)>>16));

   outb(ioPort + ATA_REG_COMMAND, ATA_CMD_READ_PIO);

   ata_delay(ioPort);
   uint8_t status = inb(ioPort + ATA_REG_STATUS);
   // poll until no longer busy
   while((status = inb(ioPort + ATA_REG_STATUS)) & ATA_STATUS_BIT_BSY);

   // poll until ready & DRQ is set, or until error
   while(!((status = inb(ioPort + ATA_REG_STATUS)) & ATA_STATUS_BIT_DRQ) && !(status & ATA_STATUS_BIT_ERR));
   if(status & ATA_STATUS_BIT_ERR) {
      gui_writestr("ATA DRIVE ERROR.", 0);
      return;
   }

   // read 256 words
   for(int i = 0; i < 256; i++) {
      buf[i] = inw(ioPort + ATA_REG_DATA);
   }

}

uint8_t *ata_read_exact(bool primaryBus, bool masterDrive, uint32_t addr, uint32_t bytes) {
   uint32_t lba = addr/512;
   uint32_t offset = addr - lba*512;
   uint32_t extraBytes = 512 - offset;

   uint32_t bytesRequired = bytes + extraBytes;

   uint16_t *buf1 = malloc(bytesRequired);

   int reads = (bytesRequired + (512-1))/512;
   for(int i = 0; i < reads; i++) {
      ata_read(primaryBus, masterDrive, lba+i, &buf1[256*i]);
   }

   return (uint8_t *) (&buf1[0]) + offset;
}