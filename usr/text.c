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

volatile int content_height = 0;

void resize(uint32_t fb, uint32_t w, uint32_t h) {
   (void)fb;
   clear();
   if(wo_text_o->cursory < (int)h - 20)
      wo_text_o->height = h - 20;
   else if(wo_text_o->height < 100)
      wo_text_o->height = 100; // minimum height
   else
      wo_text_o->height = wo_text_o->cursory + 12;
   content_height = wo_text_o->height + 20;
   w = set_content_height(content_height);

   wo_text_o->width = w - 10;
   int padding = 2;
   int x = wo_open_o->x + wo_open_o->width + padding;
   wo_path_o->width = (((w - 10) - x) * 2) / 3;
   x += wo_path_o->width + padding;

   wo_status_o->width = wo_path_o->width/2;
   wo_status_o->x = x;

   redraw();
   end_subroutine();
}

void return_fn(void *wo) {
   (void)wo;
   if(wo_text_o->cursory > wo_text_o->height - 2) {
      wo_text_o->height = wo_text_o->cursory + 12;
      content_height = wo_text_o->height + 25;
      set_content_height(content_height);
      scroll_to(wo_text_o->cursory);
   }
   clear();
   redraw();
   end_subroutine();
}

void load_file(char *filepath) {
   fat_dir_t *entry = (fat_dir_t*)fat_parse_path(filepath, true);
   if(entry == NULL) {
      display_popup("Error", "File not found");
      wo_text_o->textpos = 0;
      wo_text_o->text[wo_text_o->textpos] = '\0';
      return;
   }
   set_text((windowobj_t*)wo_path_o, filepath);
   uinttostr(entry->fileSize, wo_status_o->text);
   wo_text_o->textpos = strlen(wo_status_o->text);
   wo_text_o->cursor_textpos = wo_text_o->textpos;

   wo_text_o->text = (char*)fat_read_file(filepath);
   wo_text_o->textpos = entry->fileSize;
   wo_text_o->cursor_textpos = entry->fileSize;
   wo_text_o->text[wo_text_o->textpos] = '\0';
   wo_text_o->return_func = return_fn; // see what happens
}

void path_return() {
   load_file(wo_path_o->text);
   end_subroutine();
}

void save_func(void *wo, void *regs) {
   (void)wo;
   (void)regs;
   set_text((windowobj_t*)wo_status_o, "Saving...");

   int err = fat_write_file(wo_path_o->text, (uint8_t*)wo_text_o->text, strlen(wo_text_o->text));
   if(err < 0) {
      if(err == -1) {
         display_popup("Error", "File not found");
      } else if(err == -2) {
         display_popup("Error", "No free clusters");
      } else if(err == -3) {
         display_popup("Error", "Error updating directory entry");
      } else {
         display_popup("Error", "Unknown error");
      }
      set_text((windowobj_t*)wo_status_o, "Error");
   } else {
      set_text((windowobj_t*)wo_status_o, "Saved");
   }

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

   create_scrollbar(NULL);

   int width = get_width();
   int height = get_height();
   content_height = height;
   set_content_height(content_height);

   int x = 5;
   int padding = 2;

   wo_save_o = create_button(x, 2, "Save");
   wo_save_o->click_func = &save_func;
   x += wo_save_o->width + padding;

   wo_open_o = create_button(x, 2, "Browse");
   wo_open_o->click_func = &open_func;
   x += wo_open_o->width + padding;

   wo_path_o = create_text(x, 2, "<path>");
   wo_path_o->width = (((width - 10) - x) * 2) / 3;
   wo_path_o->return_func = &path_return;
   wo_path_o->textvalign = true;
   wo_path_o->oneline = true;
   x += wo_path_o->width + padding;

   wo_status_o = create_text(x, 2, "New");
   wo_status_o->disabled = true;
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