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

void strcpy(char* dest, char* src) {
   int i = 0;

   while(src[i] != '\0') {
      dest[i] = src[i];
      i++;
   }
   dest[i] = '\0';
}

int strlen(char* str) {
   int len = 0;
   while(str[len] != '\0')
      len++;
   return len;
}

void uinttostr(uint32_t num, char* out) {
   if(num == 0) {
      out[0] = '0';
      out[1] = '\0';
      return;
   }

   // get number length in digits
   uint32_t tmp = num;
   int length = 0;
   while(tmp > 0) {
      length++;
      tmp/=10;
   }
   
   out[length] = '\0';

   for(int i = 0; i < length; i++) {
      out[length-i-1] = '0' + num%10;
      num/=10;
   }
}

volatile windowobj_t *wo_path_o;
volatile windowobj_t *wo_text_o;
volatile windowobj_t *wo_status_o;

void load_file(char *filepath) {
   fat_dir_t *entry = (fat_dir_t*)fat_parse_path(filepath);
   if(entry == NULL) {
      write_str("File not found\n");
      exit(0);
   }
   strcpy(wo_path_o->text, filepath);
   wo_path_o->textpos = strlen(filepath);
   wo_text_o->text = (char*)fat_read_file(entry->firstClusterNo, entry->fileSize);
   uinttostr(entry->fileSize, wo_status_o->text);
   wo_text_o->textpos = entry->fileSize;
   wo_text_o->text[wo_text_o->textpos] = '\0';
}

void path_return() {
   load_file(wo_path_o->text);
   end_subroutine();
}

void save_func() {
   strcpy(wo_status_o->text, "Saved");

   fat_dir_t *entry = (fat_dir_t*)fat_parse_path(wo_path_o->text);
   fat_write_file(entry->firstClusterNo, (uint8_t*)wo_text_o->text);

   end_subroutine();
}

void _start(int argc, char **args) {
   write_str("Text\n");
   override_draw((uint32_t)NULL);
   clear(0xFFFF);

   windowobj_t *wo_save = register_windowobj();
   wo_save->type = WO_BUTTON;
   wo_save->text = (char*)malloc(1);
   strcpy(wo_save->text, "Save");
   wo_save->textpadding = 2;
   wo_save->x = 5;
   wo_save->y = 2;
   wo_save->width = 30;
   wo_save->height = 12;
   wo_save->click_func = &save_func;

   windowobj_t *wo_path = register_windowobj();
   wo_path->type = WO_TEXT;
   wo_path->text = (char*)malloc(1);
   strcpy(wo_path->text, "<New file>");
   wo_path->textpadding = 2;
   wo_path->x = 40;
   wo_path->y = 2;
   wo_path->width = 300;
   wo_path->height = 12;
   wo_path->return_func = &path_return;
   wo_path_o = wo_path;

   windowobj_t *wo_status = register_windowobj();
   wo_status->type = WO_TEXT;
   wo_status->text = (char*)malloc(1);
   wo_status->text[0] = '\0';
   wo_status->textpadding = 2;
   wo_status->x = 345;
   wo_status->y = 2;
   wo_status->width = 80;
   wo_status->height = 12;
   wo_status_o = wo_status;


   windowobj_t *wo_text = register_windowobj();
   wo_text->type = WO_TEXT;
   wo_text->text = (char*)malloc(1);
   wo_text->text[0] = '\0';
   wo_text->textpadding = 2;
   wo_text->x = 5;
   wo_text->y = 15;
   wo_text->width = get_width() - 10;
   wo_text->height = get_height() - 20;
   wo_text_o = wo_text;

   if(argc == 1) {
      load_file(args[0]);
   }

   while(true) {
      asm volatile("nop");
   }
}