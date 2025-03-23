#include <stdint.h>
#include <stdbool.h>

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
   write_str("Text\n");
   override_draw((uint32_t)NULL);
   clear(0xFFFF);

   windowobj_t *wo_text = register_windowobj();
   wo_text->type = WO_TEXT;
   wo_text->text = (char*)malloc(1);
   wo_text->text[0] = '\0';
   wo_text->textpadding = 2;
   wo_text->x = 10;
   wo_text->y = 10;
   wo_text->width = get_width() - 20;
   wo_text->height = get_height() - 20;

   if(argc == 1) {
      fat_dir_t *entry = (fat_dir_t*)fat_parse_path(args[0]);
      if(entry == NULL) {
         write_str("File not found\n");
         exit(0);
      }
      wo_text->text = (char*)fat_read_file(entry->firstClusterNo, entry->fileSize);
      wo_text->textpos = entry->fileSize;
      wo_text->text[wo_text->textpos] = '\0';
   }

   while(true) {
      asm volatile("nop");
   }
}