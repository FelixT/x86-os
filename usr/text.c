#include <stdint.h>
#include <stdbool.h>

#include "prog.h"
#include "prog_wo.h"
#include "../lib/string.h"
#include "lib/wo_api.h"
#include "lib/stdio.h"

windowobj_t *wo_menu_o;
windowobj_t *wo_path_o;
windowobj_t *wo_text_o;
windowobj_t *wo_status_o;
windowobj_t *wo_save_o;
windowobj_t *wo_open_o;

int content_height = 0;

FILE *current_file = NULL;

void resize(uint32_t fb, uint32_t w, uint32_t h) {
   (void)fb;
   (void)h;
   clear();

   wo_menu_o->width = w;
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
   FILE *f = fopen(filepath, "r");
   if(!f) {
      char buffer[250];
      sprintf(buffer, "File '%s' not found", filepath);
      display_popup("Error", buffer, false, NULL);
      wo_text_o->textpos = 0;
      wo_text_o->text[wo_text_o->textpos] = '\0';
      return;
   }

   int size = fsize(fileno(f));
   current_file = f;

   set_text((windowobj_t*)wo_path_o, filepath);
   uinttostr(size, wo_status_o->text);
   wo_status_o->textpos = strlen(wo_status_o->text);
   wo_status_o->cursor_textpos = wo_status_o->textpos;

   char *txtbuffer = (char*)malloc(size+1);
   fread(txtbuffer, size, 1, f);

   wo_text_o->text = txtbuffer;
   wo_text_o->textpos = size;
   wo_text_o->cursor_textpos = size;
   wo_text_o->text[wo_text_o->textpos] = '\0';
   wo_text_o->return_func = return_fn;
   
   redraw();
   wo_text_o->height = wo_text_o->cursory + 10;
   clear();
   redraw();
   content_height = wo_text_o->height + 25;
   set_content_height(content_height);
}

void path_return() {
   load_file(wo_path_o->text);
   end_subroutine();
}

void save_func(void *wo, void *regs) {
   (void)wo;
   (void)regs;
   set_text((windowobj_t*)wo_status_o, "Saving...");

   int w = fwrite(wo_path_o->text, strlen(wo_text_o->text), 1, current_file);
   debug_printf("Wrote %i bytes\n", w);
   if(w <= 0 && strlen(wo_text_o->text) > 0) {
      display_popup("Error", "Write failed", false, NULL);
      set_text((windowobj_t*)wo_status_o, "Error");
   } else {
      fflush(current_file);
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

   int height = get_height();
   content_height = height;
   int width = get_width();

   int x = 5;
   int padding = 2;

   wo_menu_o = register_windowobj(WO_CANVAS, 0, 0, width, 20);
   wo_menu_o->bordered = false;

   wo_save_o = create_button(wo_menu_o, x, 2, "Save");
   wo_save_o->click_func = &save_func;
   x += wo_save_o->width + padding;

   wo_open_o = create_button(wo_menu_o, x, 2, "Browse");
   wo_open_o->click_func = &open_func;
   x += wo_open_o->width + padding;

   wo_path_o = create_text(wo_menu_o, x, 2, "<path>");
   wo_path_o->width = (((width - 10) - x) * 2) / 3;
   wo_path_o->return_func = &path_return;
   wo_path_o->textvalign = true;
   wo_path_o->oneline = true;
   x += wo_path_o->width + padding;

   wo_status_o = create_text(wo_menu_o, x, 2, "New");
   wo_status_o->disabled = true;
   wo_status_o->width = wo_path_o->width/2;
   wo_status_o->textvalign = true;
   wo_status_o->texthalign = true;

   wo_text_o = create_text(NULL, 5, 22, "");
   //resize() // resize text buffer
   wo_text_o->width = width - 10;
   wo_text_o->height = height - 20;

   if(argc == 1 && *args[0] != '\0') {
      if(args[0][0] != '/') {
         // relative path
         char path[256];
         getwd(path);
         if(!strequ(path, "/"))
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