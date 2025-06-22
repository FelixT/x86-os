#include <stdint.h>
#include <stdbool.h>

#include "prog.h"
#include "prog_wo.h"
#include "../lib/string.h"
#include "lib/wo_api.h"

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
volatile windowobj_t *wo_save_o;
volatile windowobj_t *wo_open_o;

void resize(uint32_t fb, uint32_t w, uint32_t h) {
   (void)fb;
   clear();
   wo_text_o->width = w - 10;
   wo_text_o->height = h - 20;

   int padding = 2;
   int x = wo_open_o->x + wo_open_o->width + padding;
   wo_path_o->width = (((w - 10) - x) * 2) / 3;
   x += wo_path_o->width + padding;

   wo_status_o->width = wo_path_o->width/2;
   wo_status_o->x = x;

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
      display_popup("Error", "File not found");
      wo_text_o->textpos = 0;
      wo_text_o->text[wo_text_o->textpos] = '\0';
      return;
   }
   strcpy(wo_path_o->text, filepath);
   wo_path_o->textpos = strlen(filepath);
   wo_text_o->text = (char*)fat_read_file(filepath);
   uinttostr(entry->fileSize, wo_status_o->text);
   wo_text_o->textpos = entry->fileSize;
   wo_text_o->text[wo_text_o->textpos] = '\0';
}

void path_return() {
   load_file(wo_path_o->text);
   end_subroutine();
}

void save_func(void *wo, void *regs) {
   (void)wo;
   (void)regs;
   set_status("Saving...");

   fat_write_file(wo_path_o->text, (uint8_t*)wo_text_o->text, strlen(wo_text_o->text));

   set_status("Saved");

   end_subroutine();
}

void filepicker_return(char *path) {
   load_file(path);
   end_subroutine();
}

void open_func(void *wo, void *regs) {
   (void)wo;
   (void)regs;
   display_filepicker(&filepicker_return);
   end_subroutine();
}

void _start(int argc, char **args) {
   set_window_title("Text Edit");
   
   override_draw((uint32_t)NULL);
   override_resize((uint32_t)resize);
   clear();

   int width = get_width();
   int height = get_height();

   int x = 5;
   int padding = 2;

   wo_save_o = create_button(x, 2, "Save");
   wo_save_o->click_func = &save_func;
   x += wo_save_o->width + padding;

   wo_open_o = create_button(x, 2, "Open");
   wo_open_o->click_func = &open_func;
   x += wo_open_o->width + padding;

   wo_path_o = create_text(x, 2, "<New file>");
   wo_path_o->width = (((width - 10) - x) * 2) / 3;
   wo_path_o->return_func = &path_return;
   wo_path_o->textvalign = true;
   x += wo_path_o->width + padding;

   wo_status_o = create_text(x, 2, "");
   wo_status_o->width = wo_path_o->width/2;
   wo_status_o->textvalign = true;
   wo_status_o->texthalign = true;

   wo_text_o = create_text(5, 17, "");
   //resize() // resize text buffer
   wo_text_o->width = width - 10;
   wo_text_o->height = height - 20;

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