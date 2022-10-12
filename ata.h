#ifndef ATA_H
#define ATA_H

#include "gui.h"

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

void ata_identify(bool primaryBus, bool masterDrive);
void ata_read(bool primaryBus, bool masterDrive, uint32_t lba, uint16_t *buf);
uint8_t *ata_read_exact(bool primaryBus, bool masterDrive, uint32_t addr, uint32_t bytes);

#endif