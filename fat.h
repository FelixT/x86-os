#ifndef FAT_H
#define FAT_H

#include <stdint.h>
#include <stdbool.h>

#include "memory.h"
#include "gui.h"
#include "ata.h"

// https://www.freebsd.org/cgi/man.cgi?query=newfs_msdos&apropos=0&sektion=0&manpath=FreeBSD+5.2-RELEASE&format=html
// BPB / bios parameter block
typedef struct {
   uint8_t start[3];
   uint8_t identifier[8];
   uint16_t bytesPerSector;
   uint8_t sectorsPerCluster;
   uint16_t noReservedSectors;
   uint8_t noTables;
   uint16_t noRootEntries;
   uint16_t noSectors;
   uint8_t mediaDescriptor;
   uint16_t sectorsPerFat;
   uint16_t sectorsPerTrack;
   uint16_t noDriveHeads;
   uint32_t noHiddenSectors;
   uint32_t largeNoSectors; // used if value doesn't fit noSectors

} __attribute__((packed)) fat_bpb_t;

// extended boot record
typedef struct {
   uint8_t driveNo;
   uint8_t flags;
   uint8_t signature;
   uint32_t volumeID;
   uint8_t volumeLabel[11];
   uint8_t fatTypeStr[8];
   uint32_t bootCode[112];
   uint16_t bootSignature;
} __attribute__((packed)) fat_ebr_t;

typedef struct {
   uint8_t filename[11];
   uint8_t attributes;
   uint8_t reserved;
   uint8_t creationTimeFine; // in 10ths of a second
   uint16_t creationTime;
   uint16_t creationDate;
   uint16_t lastAccessDate;
   uint16_t zero; // high 16 bits of entry's first cluster no in other fat vers
   uint16_t lastModifyTime;
   uint16_t lastModifyDate;
   uint16_t firstClusterNo;
   uint32_t fileSize; // bytes
} __attribute__((packed)) fat_dir_t;

fat_dir_t *fat_parse_path(char *path);
uint8_t *fat_read_file(uint16_t clusterNo, uint32_t size);
void fat_read_root(fat_dir_t *items);
void fat_setup();
void fat_read_dir(uint16_t clusterNo, fat_dir_t *items);
void fat_get_info();
void fat_parse_dir_entry(fat_dir_t *fat_dir);
fat_bpb_t fat_get_bpb();
int fat_get_dir_size(uint16_t clusterNo);

#endif