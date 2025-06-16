#include <stdint.h>
#include <stdbool.h>

#include "prog.h"
#include "../lib/string.h"

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

volatile windowobj_t *wo_path_o;
volatile windowobj_t *wo_text_o;
volatile windowobj_t *wo_status_o;

void resize(uint32_t fb, uint32_t w, uint32_t h) {
   (void)fb;
   clear();
   wo_text_o->width = w - 10;
   wo_text_o->height = h - 20;
   wo_path_o->width = 2*(w-10)/3;
   wo_status_o->x = 42+2*(w-10)/3;
   wo_status_o->width = (w-10)/3 - 42;
   redraw();
   end_subroutine();
}

void set_status(char *status) {
   strcpy(wo_status_o->text, status);
   wo_status_o->textpos = strlen(status);
}

void load_file(char *filepath) {
   fat_dir_t *entry = (fat_dir_t*)fat_parse_path(filepath, true);
   if(entry == NULL) {
      set_status("File not found");
      wo_text_o->textpos = 0;
      wo_text_o->text[wo_text_o->textpos] = '\0';
      return;
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
   set_status("Saving...");

   fat_write_file(wo_path_o->text, (uint8_t*)wo_text_o->text, strlen(wo_text_o->text));

   set_status("Saved");

   end_subroutine();
}

void _start(int argc, char **args) {
   set_window_title("Text Edit");
   
   override_draw((uint32_t)NULL);
   override_resize((uint32_t)resize);
   clear();

   int width = get_width();
   int height = get_height();

   windowobj_t *wo_save = register_windowobj(WO_BUTTON, 5, 2, 34, 13);
   wo_save->text = (char*)malloc(1);
   strcpy(wo_save->text, "Save");
   wo_save->textvalign = true;
   wo_save->click_func = &save_func;

   windowobj_t *wo_path = register_windowobj(WO_TEXT, 40, 2, 2*(width-10)/3, 13);
   wo_path->text = (char*)malloc(1);
   strcpy(wo_path->text, "<New file>");
   wo_path->textvalign = true;
   wo_path->return_func = &path_return;
   wo_path_o = wo_path;

   windowobj_t *wo_status = register_windowobj(WO_TEXT, 42+2*(width-10)/3, 2, (width-10)/3 - 42, 13);
   wo_status->text = (char*)malloc(1);
   wo_status->text[0] = '\0';
   wo_status->textvalign = true;
   wo_status_o = wo_status;

   windowobj_t *wo_text = register_windowobj(WO_TEXT, 5, 17, width - 10, height - 20);
   wo_text->type = WO_TEXT;
   wo_text->text = (char*)malloc(0x1000);
   wo_text->text[0] = '\0';
   wo_text_o = wo_text;

   if(argc == 1 && *args[0] != '\0') {
      if(args[0][0] != '/') {
         // relative path
         char path[256];
         getwd(path);
         if(!strcmp(path, "/"))
            strcat(path, "/");
         strcat(path, args[0]);
         load_file(path);
      } else {
         // absolute path
         load_file(args[0]);
      }
   }

   redraw();

   while(true) {
      asm volatile("nop");
   }
}