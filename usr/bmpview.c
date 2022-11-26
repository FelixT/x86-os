#include <stdint.h>

#include "prog.h"

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

void _start(int argc, char **args) {

   if(argc != 1) {
      write_str("Wrong number of arguments provided");
      exit(0);
   }

   char *path = (char*)args[0];

   fat_dir_t *entry = (fat_dir_t*)fat_parse_path(path);
   if(entry == NULL) {
      write_str("File not found");
      exit(0);
   }
   
   uint8_t *bmp = (uint8_t*)fat_read_file(entry->firstClusterNo, entry->fileSize);
   bmp_draw((uint8_t*)bmp, 0, 0);

   exit(0);

}