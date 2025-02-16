#include <stdint.h>
#include <stdbool.h>

#include "prog.h"

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

typedef struct {
   uint8_t filename[11];
   uint8_t attributes;
   uint8_t reserved;
   uint8_t creationTimeFine; // in 10ths of a second
   uint16_t creationTime;
   uint16_t creationDate;
   uint16_t lastAccessDate;
   uint16_t zero; // high 16 bits of entry's first cluster no in other fat vers. we can use this for position
   uint16_t lastModifyTime;
   uint16_t lastModifyDate;
   uint16_t firstClusterNo;
   uint32_t fileSize; // bytes
} __attribute__((packed)) fat_dir_t;

volatile uint8_t *image;

int i = 0;

void timer_callback() {
    //clear(0xFFFF);
    bmp_draw((uint8_t*)image, i%400, i%400);
    redraw();

    queue_event((uint32_t)(&timer_callback), 1);

    i+=5;

    end_subroutine();
}

void click_callback() {
    clear(0xFFFF);
    i = 0;
    timer_callback();
}

void _start() {
    write_str("Prog4\n");
    override_draw((uint32_t)NULL);

    fat_dir_t *entry = (fat_dir_t*)fat_parse_path("/bmp/file20.bmp");
    if(entry == NULL) {
        write_str("File icon not found\n");
        exit(0);
    }
    image = (uint8_t*)fat_read_file(entry->firstClusterNo, entry->fileSize);

    clear(0xFFFF);

    windowobj_t *wo = register_windowobj();
    wo->type = WO_BUTTON;
    wo->text = (char*)malloc(1);
    wo->text[0] = 'C';
    wo->text[1] = 'L';
    wo->text[2] = 'I';
    wo->text[3] = 'C';
    wo->text[4] = 'K';
    wo->text[5] = '\0';
    wo->click_func = &click_callback;

    // main program loop
   while(1 == 1) {
    asm volatile("nop");
   }
   exit(0);

}