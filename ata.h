#ifndef ATA_H
#define ATA_H

#include "gui.h"
#include "registers_t.h"

#define ATA_CMD_IDENTIFY 0xEC
#define ATA_CMD_READ_PIO 0x20
#define ATA_CMD_WRITE_PIO 0x30
#define ATA_CMD_CACHE_FLUSH 0xE7

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

#define ATA_CTRL_REG_ALT_STATUS 0x00 // read
#define ATA_CTRL_REG_DEV_CONTROL 0x00 // write
#define ATA_CTRL_SRST 0x04 // software reset, resets both drives on the bus

#define ATA_STATUS_BIT_ERR 0x01 // error
#define ATA_STATUS_BIT_DRQ 0x08 // data request, drive has data ready / wants data
#define ATA_STATUS_BIT_SRV 0x10 // service request
#define ATA_STATUS_BIT_DF 0x20 // drive fail
#define ATA_STATUS_BIT_RDY 0x40 // ready
#define ATA_STATUS_BIT_BSY 0x80 // busy

#define ATA_SECTOR_SIZE 512 // bytes
#define ATA_MAX_SECTORS 256 // max sectors per r/w op (8-bit count reg, 0 encodes 256)

#define ATA_POLL_SPINS 10000000
#define ATA_DRAIN_SPINS (ATA_MAX_SECTORS*256)
#define ATA_RESET_DELAYS 20

void ata_identify(bool primaryBus, bool masterDrive);
void ata_recover(uint16_t ioPort);
uint8_t *ata_read_exact(bool primaryBus, bool masterDrive, uint32_t addr, uint32_t bytes);
bool ata_read_exact_into(bool primaryBus, bool masterDrive, uint32_t addr, uint32_t bytes, uint8_t *outBuf);
bool ata_write_exact(bool primaryBus, bool masterDrive, uint32_t addr, uint8_t *buf, int size);
void ata_interrupt(registers_t *regs);

#endif