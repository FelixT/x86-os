#include <stdint.h>
#include <stdbool.h>

#include "prog.h"
#include "../lib/string.h"
#include "lib/stdio.h"
#include "lib/stdlib.h"
#include "lib/dialogs.h"
#include "lib/ui/ui_textarea.h"

wo_t *wo_menu;
wo_t *wo_path;
wo_t *wo_text;
wo_t *wo_status;
wo_t *wo_save;
wo_t *wo_open;
wo_t *wo_new;

int content_height = 0;

FILE *current_file = NULL;

dialog_t *dialog;
ui_mgr_t *ui;

void resize(uint32_t fb, uint32_t w, uint32_t h) {
   (void)fb;
   (void)h;
   clear();
   *ui->surface = get_surface();

   wo_menu->width = w;
   wo_text->width = w - 4;
   int padding = 2;
   int x = wo_path->x;
   wo_path->width = ((w - 10 - x) * 2) / 3;
   x += wo_path->width + padding;

   wo_status->width = wo_path->width/2;
   wo_status->x = x;

   ui_draw(ui);
   redraw();
   end_subroutine();
}

void text_keypress(wo_t *wo, uint16_t c, int window) {
   // resize
   keypress_textarea(wo, c, window);
   int height = textarea_get_rows(wo) * (get_font_info().height + get_font_info().padding) + 6;
   if(height != wo->height) {
      wo->height = height;
      content_height = height + 25;
      set_content_height(content_height, -1);
      int row, col;
      textarea_get_pos_from_index(wo, get_textarea(wo)->cursor_pos, &row, &col);
      scroll_to(25 + row*(get_font_info().height + get_font_info().padding) + 6, -1);
      clear();
      ui_draw(ui);
      redraw();
   }
}

void load_file(char *filepath) {
   FILE *f = fopen(filepath, "r+");
   if(!f) {
      char buffer[250];
      sprintf(buffer, "File '%s' not found", filepath);
      dialog_msg("Error", buffer);
      set_textarea_text(wo_text, "");
      return;
   }

   int size = fsize(fileno(f));
   current_file = f;

   set_input_text(wo_path, filepath);
   get_input(wo_path)->placeholder = false;
   char buffer[8];
   uinttostr(size, buffer);
   set_input_text(wo_status, buffer);

   char *txtbuffer = (char*)malloc(size+1);
   fread(txtbuffer, size, 1, f);
   txtbuffer[size] = '\0';
   set_textarea_text(wo_text, txtbuffer);
   
   int height = textarea_get_rows(wo_text) * (get_font_info().height + get_font_info().padding) + 6;
   wo_text->height = height;
   clear();
   ui_draw(ui);
   redraw();
   content_height = wo_text->height + 25;
   set_content_height(content_height, -1);
}

void path_return(wo_t *wo, int window) {
   (void)window;
   load_file(get_input(wo)->text);
}

void error(char *msg) {
   dialog_msg("Error", msg);
   set_input_text(wo_status, "Error");
}

void save_func(wo_t *wo, int window) {
   (void)wo;
   (void)window;
   set_input_text(wo_status, "Saving...");
   if(current_file == NULL) {
      // new file
      if(get_input(wo_path)->text[0] == '/') {
         FILE *f = fopen(get_input(wo_path)->text, "w");
         if(!f) {
            error("Couldn't create file");
         }
         current_file = f;
      } else {
         char buffer[256];
         sprintf(buffer, "Invalid path '%s'", get_input(wo_path)->text);
         error(buffer);
      }
   }

   fseek(current_file, 0, SEEK_SET);
   int w = fwrite(get_textarea(wo_text)->text, strlen(get_textarea(wo_text)->text), 1, current_file);
   debug_println("Wrote %i bytes %i", w, strlen(get_textarea(wo_text)->text));
   if(w <= 0 && strlen(get_textarea(wo_text)->text) > 0) {
      error("Write failed");
   } else {
      fflush(current_file);
      set_input_text(wo_status, "Saved");
   }
}

void filepicker_return(char *path, int window) {
   (void)window;
   load_file(path);
}

void open_func(wo_t *wo, int window) {
   (void)wo;
   (void)window;
   dialog_filepicker("/", &filepicker_return);
}

void new_func(wo_t *wo, int window) {
   (void)wo;
   (void)window;
   launch_task("/sys/text.elf", 0, NULL, false);
}

void scroll(int deltaY, int offsetY, int window) {
   (void)window;
   ui_scroll(ui, deltaY, offsetY);
   ui_draw(ui);
   redraw();
   end_subroutine();
}

void _start(int argc, char **args) {

   int index = get_free_dialog();
   dialog = get_dialog(index);
   dialog_init(dialog, -1);
   dialog_set_title(dialog, "Text Edit");
   ui = dialog->ui;
   
   override_resize(resize, -1);
   clear();

   create_scrollbar(&scroll, -1);

   int height = get_height();
   content_height = height;
   int width = get_width();

   int x = 5;
   int padding = 2;

   wo_menu = create_canvas(0, 0, width, 20);
   ui_add(ui, wo_menu);

   wo_save = create_button(x, 2, 30, 16, "Save");
   wo_save->width = 35;
   set_button_release(wo_save, &save_func);
   x += wo_save->width + padding;
   canvas_add(wo_menu, wo_save);

   wo_open = create_button(x, 2, 30, 16, "Open");
   wo_open->width = 35;
   set_button_release(wo_open, &open_func);
   x += wo_open->width + padding;
   canvas_add(wo_menu, wo_open);

   wo_new = create_button(x, 2, 30, 16, "New");
   wo_new->width = 35;
   set_button_release(wo_new, &new_func);
   x += wo_new->width + padding;
   canvas_add(wo_menu, wo_new);

   wo_path = create_input(x, 2, ((width - 10 - x) * 2) / 3, 16);
   set_input_placeholder(wo_path, "<new>");
   set_input_return(wo_path, &path_return);
   get_input(wo_path)->valign = true;
   get_input(wo_path)->halign = true;
   x += wo_path->width + padding;
   canvas_add(wo_menu, wo_path);

   wo_status = create_input(x, 2, wo_path->width/2, 16);
   set_input_text(wo_status, "New");
   wo_status->focusable = false;
   get_input(wo_status)->valign = true;
   get_input(wo_status)->halign = true;
   canvas_add(wo_menu, wo_status);

   wo_text = create_textarea(2, 22, width - 4, get_font_info().height + get_font_info().padding + 6);
   wo_text->keypress_func = &text_keypress;
   ui_add(ui, wo_text);

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

   ui_draw(ui);
   redraw();

   while(true) {
      asm volatile("nop");
   }
}