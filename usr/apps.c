// read /sys folder for .elfs

#include <stddef.h>
#include <stdbool.h>

#include "prog.h"
#include "lib/stdio.h"
#include "../lib/string.h"
#include "lib/wo_api.h"
#include "lib/sort.h"

void app_launch(void *wo, void *regs) {
   (void)regs;
   windowobj_t *w = (windowobj_t*)wo;
   w->clicked = false;
   w->hovering = false;
   w->bordered = false;
   char path[255];
   strcpy(path, "/sys/");
   strcat(path, w->text);
   strcat(path, ".elf");
   debug_println(path);
   redraw();
   launch_task(path, 0, NULL, false);
   end_subroutine();
}

int sort_filename(const void *v1, const void *v2) {
   const fs_dir_entry_t *d1 = (const fs_dir_entry_t*)v1;
   const fs_dir_entry_t *d2 = (const fs_dir_entry_t*)v2;

   return strcmp(d1->filename, d2->filename);
}

void _start() {
   
   set_window_size(120, 280);
   set_window_title("Apps");

   fs_dir_content_t *content = read_dir("/sys");
   if(content->entries)
      sort(content->entries, content->size, sizeof(fs_dir_entry_t), sort_filename);
   override_draw(0);

   int y = 5;
   for(int i = 0; i < content->size; i++) {
      fs_dir_entry_t *entry = &content->entries[i];
      if(strendswith(entry->filename, ".elf")) {
         char name[10];
         strsplit(name, NULL, entry->filename, '.');
         windowobj_t *wo = create_text_static(NULL, 20, y, name);
         wo->colour_text = rgb16(10, 30, 100);
         wo->release_func = (void*)&app_launch;
         wo->cursor_textpos = strlen(name);
         wo->width = 75;

         // draw line
         surface_t surface = get_surface();
         for(int x = 15; x < 100; x++)
            ((uint16_t*)surface.buffer)[(y+15)*surface.width+x] = rgb16(200, 200, 200);

         y += 20;
      }
   }

   free(content, sizeof(fs_dir_content_t) * content->size);

   create_scrollbar(NULL);
   set_content_height(y);

   redraw();
   
   while(true) {
      yield();
   }

   exit(0);
}