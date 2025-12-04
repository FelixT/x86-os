// read /sys folder for .elfs

#include <stddef.h>
#include <stdbool.h>

#include "prog.h"
#include "lib/stdio.h"
#include "../lib/string.h"
#include "lib/ui/ui_mgr.h"
#include "lib/ui/ui_button.h"
#include "lib/ui/ui_label.h"
#include "lib/sort.h"
#include "lib/draw.h"

int items = 0;
ui_mgr_t *ui = NULL;
surface_t surface;

void app_launch(wo_t *label) {
   label_t *label_data = (label_t *)label->data;  
   if(!label_data)
      return;
   char path[256];
   strcpy(path, "/sys/");
   strcat(path, label_data->label);
   strcat(path, ".elf");
   debug_println(path);
   redraw();
   launch_task(path, 0, NULL, false);
}

int sort_filename(const void *v1, const void *v2) {
   const fs_dir_entry_t *d1 = (const fs_dir_entry_t*)v1;
   const fs_dir_entry_t *d2 = (const fs_dir_entry_t*)v2;

   return strcmp(d1->filename, d2->filename);
}

void scroll(int deltaY, int offsetY) {
   (void)deltaY;

   clear();

   int offsetRemainder = (offsetY-5) % 20;

   int y = 5;

   for(int i = 0; i < items; i++) {
      ui->wos[i]->y = y - offsetY;
      // draw line
      int lineY = y + 12 - offsetRemainder;
      if(lineY >= 0 && lineY < surface.height)
         draw_line(&surface, rgb16(230, 230, 230), 15, y+12 - offsetRemainder, false, 85);

      y += 20;
   }
   if(ui)
      ui_draw(ui);
   redraw();

   end_subroutine();
}

void click(int x, int y, int window) {
   (void)window;
   if(!ui)
      end_subroutine();

   ui_click(ui, x, y);

   end_subroutine();
}

void release(int x, int y, int window) {
   (void)window;
   if(!ui)
      end_subroutine();

   ui_release(ui, x, y);

   end_subroutine();
}

void resize() {
   surface = get_surface();
   ui->surface = &surface;
   ui_draw(ui);
   int y = 5;
   // draw lines
   for(int i = 0; i < items; i++) {
      int lineY = y + 17;
      if(lineY < surface.height)
         draw_line(&surface, rgb16(230, 230, 230), 15, lineY, false, 85);

      y += 20;
   }
   end_subroutine();
}

void _start() {
   
   set_window_size(120, 280);
   set_window_title("Apps");

   fs_dir_content_t *content = read_dir("/sys");
   if(content->entries)
      sort(content->entries, content->size, sizeof(fs_dir_entry_t), sort_filename);
   override_draw(0);
   override_click((uint32_t)&click, -1);
   override_release((uint32_t)&release, -1);
   override_resize((uint32_t)&resize);

   items = 0;
   int y = 5;

   surface = get_surface();
   ui = ui_init(&surface, -1);

   for(int i = 0; i < content->size; i++) {
      fs_dir_entry_t *entry = &content->entries[i];
      if(strendswith(entry->filename, ".elf")) {
         char name[10];
         strsplit(name, NULL, entry->filename, '.');
         wo_t *wo = create_label(20, y, 75, 16, name);
         label_t *label = (label_t *)wo->data;
         label->colour_txt = rgb16(10, 30, 100);
         label->release_func = (void*)&app_launch;
         ui_add(ui, wo);

         // draw line
         int lineY = y + 17;
         if(lineY < surface.height)
            draw_line(&surface, rgb16(230, 230, 230), 15, lineY, false, 85);

         y += 20;
         items++;
      }
   }

   ui_draw(ui);

   free(content, sizeof(fs_dir_content_t) * content->size);

   create_scrollbar(&scroll);
   set_content_height(y);

   redraw();
   
   while(true) {
      yield();
   }

   exit(0);
}