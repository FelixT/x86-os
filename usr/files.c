#include <stdint.h>
#include <stdbool.h>
#include "../lib/string.h"
#include "lib/stdio.h"
#include "lib/wo_api.h"

#include "prog.h"
#include "prog_fat.h"
#include "prog_wo.h"

volatile uint16_t *framebuffer;
volatile uint32_t width;
volatile uint32_t height;
volatile uint8_t *file_icon;
volatile uint8_t *folder_icon;
volatile int offset;
fs_dir_content_t *dir_content;
int shown_items;

char cur_path[512];
windowobj_t *wo_menu;
windowobj_t *wo_path;
windowobj_t *wo_newfile;
windowobj_t *wo_newfolder;

char tolower_c(char c) {
   if(c >= 'A' && c <= 'Z')
      c += ('a'-'A');
   return c;
}

void get_abs_path(char *out, char *inpath) {
   if(inpath[0] == '/') {
      // absolute path
      strcpy(out, inpath);
   } else {
      // relative path
      strcpy(out, cur_path);
      if(!strequ(out, "/"))
         strcat(out, "/");
      strcat(out, inpath);
   }
}

void display_items() {
   int y = 5;
   int x = 5;

   clear();
   
   int offsetLeft = offset;
   int position = 0;
   shown_items = 0;

   for(int i = 0; i < dir_content->size; i++) {
      fs_dir_entry_t *entry = &dir_content->entries[i];
      bool dotentry = entry->filename[0] == '.';

      if(entry->hidden && !dotentry) {
         continue; // ignore
      }
      
      shown_items++;
      offsetLeft--;
      if(offsetLeft >= 0) {
         continue;
      }


      // draw
      if(entry->type == FS_TYPE_DIR) {
         bmp_draw((uint8_t*)folder_icon, x, y, 1, true);
         write_strat(entry->filename, x + 27, y + 7);
      } else {
         bmp_draw((uint8_t*)file_icon, x, y, 1, true);
         write_strat(entry->filename, x + 27, y + 7);

         uint32_t size = entry->file_size;
         char type[4];
         strcpy(type, "b");
         if(size > 1000) {
            strcpy(type, "kb");
            size /= 1000;
            if(size > 1000) {
               strcpy(type, "mb");
               size /= 1000;
            }
         }
         char sizeStr[20];
         sprintf(sizeStr, "<%u %s>", size, type);
         write_strat(sizeStr, x + 150, y + 7);
      }

      y += 25;
      position++;
   }

   set_content_height(y + 25*offset + 10);

   set_text(wo_path, cur_path);
}

void read_root() {
   strcpy(cur_path, "/");
   // read root
   dir_content = read_dir(cur_path);
}

void uparrow() {

   offset--;
   if(offset < 0) offset = 0;

   display_items();
   redraw();

   end_subroutine();

}

void downarrow() {

   offset++;
   if(offset >= shown_items) offset = shown_items - 1;

   display_items();
   redraw();

   end_subroutine();

}

void path_callback() {
   if(wo_path->text == NULL) {
      debug_write_str("Text is null\n");
      end_subroutine();
      return;
   }

   char path[500];
   strcpy(path, wo_path->text);
   int len = strlen(path);
   if(len > 0 && path[len-1] != '/') {
      strcat(path, "/");
   }
   fat_dir_t *dir = (fat_dir_t*)fat_parse_path(path, false);

   if(!dir) {
      if(strequ(path, "/")) {
         read_root();
         display_items();
      } else {
         char buffer[500];
         sprintf(buffer, "Location '%s' not found", path);
         display_popup("Error", buffer, false, NULL);
      }
   } else {
      strcpy(cur_path, wo_path->text);
      dir_content = read_dir(cur_path);
      offset = 0;
      display_items();
   }

   end_subroutine();
}

void click(int x, int y) {

   (void)(x);
   // clicked scrollbar
   if(x > (int)width - 20) {
      end_subroutine();
      return;
   }

   // clicked menu
   if(y > (int)height - 20) {
      end_subroutine();
      return;
   }

   // see where we clicked
   int position = (y-5)/25;

   if(dir_content == NULL) end_subroutine();

   int index = -1;
   int i_pos = 0;
   for(int i = 0; i < dir_content->size; i++) {
      fs_dir_entry_t *entry = &dir_content->entries[i];
      bool dotentry = entry->filename[0] == '.';
      if(entry->hidden && !dotentry)
         continue;
      if(i_pos == position)
         index = i;
      i_pos++;
   }

   if(index < 0)
      end_subroutine();

   fs_dir_entry_t *clicked_entry = &dir_content->entries[index];

   if(clicked_entry->type == FS_TYPE_DIR) {
      offset = 0;
      scroll_to(0);

      // update path
      if(clicked_entry->filename[0] == '.') {
         if(clicked_entry->filename[1] == '.') {
            int lastslashpos = -1;
            for(int i = 0; cur_path[i] != '\0'; i++) {
               if(cur_path[i] == '/')
                  lastslashpos = i;
            }

            if(lastslashpos > 0) {
               cur_path[lastslashpos] = '\0';
            } else if(lastslashpos == 0) {
               cur_path[1] = '\0';
            }
         } else {
            // do nothing
         }
      } else {
         int pi = strlen(cur_path);
         if(pi == 1) pi = 0;
         cur_path[pi] = '/';
         cur_path[pi+1] = '\0';
         strcat(cur_path, clicked_entry->filename);
      }

      // read dir
      dir_content = read_dir(cur_path);
      display_items();
      redraw();

   } else {
      // file
      char extension[4];
      strsplit(NULL, extension, clicked_entry->filename, '.');

      char fullpath[255];
      int pi = strlen(cur_path);
      for(int x = 0; x < pi; x++)
         fullpath[x] = cur_path[x];
      fullpath[pi] = '/';
      fullpath[pi+1] = '\0';
      strcat(fullpath, clicked_entry->filename);

      bool launched = false;
      // handle supported extensions
      // bmp, elf, txt
      if(strequ(extension, "bmp")) {
         char **args = (char**)malloc(1*sizeof(char*));
         args[0] = malloc(strlen(fullpath)+1);
         strcpy(args[0], fullpath);

         // note: this also ends the subroutine
         launched = true;
         launch_task("/sys/bmpview.elf", 1, args);
      }

      if(strequ(extension, "elf")) {
         // note: this also ends the subroutine
         launched = true;
         launch_task(fullpath, 0, NULL);
      }

      if(strequ(extension, "txt") || strequ(extension, "c")) {
         char **args = (char**)malloc(1*sizeof(char*));
         args[0] = malloc(strlen(fullpath)+1);
         strcpy(args[0], fullpath);

         // note: this also ends the subroutine
         launched = true;
         launch_task("/sys/text.elf", 1, args);
      }

      if(strequ(extension, "fon")) {
         set_sys_font(fullpath);
      }

      if(launched) {
         debug_printf("This should never happen!"); // launching task should end the subroutine
         while(true) {}
      }

   }

   end_subroutine();

}

void resize(uint32_t fb, uint32_t w, uint32_t h) {
   (void)w;
   framebuffer = (uint16_t*)fb;
   width = get_surface().width;
   height = h;

   wo_menu->width = w;
   wo_menu->y = height - 20;
   wo_path->width = w - 120;
   wo_newfile->x = w - 115;
   wo_newfolder->x = w - 63;

   display_items();
   redraw();
   end_subroutine();
}

void scroll(int deltaY, int offsetY) {
   (void)deltaY;
   offset = (offsetY + 24) / 25;
   if(offset >= shown_items) offset = shown_items - 1;
   display_items();
   redraw();
   end_subroutine();
}

void add_file_callback(char *filename) {
   if(filename && !strequ(filename, "")) {
      debug_write_str(filename);
      char path[512];
      get_abs_path(path, filename);
      debug_write_str(path);
      int fd = new_file(path);
      if(fd < 0) {
         char buffer[250];
         sprintf(buffer, "Failed to create file '%s'", path);
         display_popup("Error", buffer, false, NULL);
      } else {
         close(fd);
      }
      // refresh
      dir_content = read_dir(cur_path);
      display_items();
      redraw();
   }
   end_subroutine();
}

void add_file(void *wo, void *regs) {
   (void)wo;
   (void)regs;
   display_popup("Name", "Enter filename", true, &add_file_callback);
   end_subroutine();
}

void add_folder_callback(char *name) {
   if(name && !strequ(name, "")) {
      debug_write_str(name);
      char path[512];
      get_abs_path(path, name);
      debug_write_str(path);
      // actually add the dir
      if(!mkdir(path)) {
         char buffer[250];
         sprintf(buffer, "Failed to create folder '%s'", path);
         display_popup("Error", buffer, false, NULL);
      }
      // refresh
      dir_content = read_dir(cur_path);
      display_items();
      redraw();
   }
   end_subroutine();
}

void add_folder(void *wo, void *regs) {
   (void)wo;
   (void)regs;
   display_popup("Name", "Enter folder name", true, &add_folder_callback);
   end_subroutine();
}

void _start(int argc, char **args) {
   (void)argc;
   (void)args;

   // init
   set_window_title("File Manager");

   dir_content = read_dir("/");
   offset = 0;
   file_icon = (uint8_t*)fat_read_file("/bmp/file20.bmp");
   if(file_icon == NULL) {
      write_str("File icon not found\n");
      exit(0);
   }

   folder_icon = (uint8_t*)fat_read_file("/bmp/folder20.bmp");
   if(folder_icon == NULL) {
      write_str("Folder icon not found\n");
      exit(0);
   }

   framebuffer = (uint16_t*)(get_surface().buffer);

   create_scrollbar(&scroll);
   override_uparrow((uint32_t)&uparrow);
   override_downarrow((uint32_t)&downarrow);
   override_click((uint32_t)&click);
   override_draw((uint32_t)NULL);
   override_resize((uint32_t)&resize);
   width = get_surface().width;
   height = get_height();

   int displayedwidth = get_width();

   wo_menu = register_windowobj(WO_CANVAS, 0, height - 20, displayedwidth, 20);
   wo_menu->bordered = false;
   wo_path = create_text(wo_menu, 4, 3, "/");
   wo_path->width = displayedwidth - 120;
   wo_path->return_func = &path_callback;
   wo_path->oneline = true;
   wo_newfile = create_button(wo_menu, displayedwidth - 114, 3, "+ File");
   wo_newfile->width = 50;
   wo_newfile->click_func = &add_file;

   wo_newfolder = create_button(wo_menu, displayedwidth - 63, 3, "+ Folder");
   wo_newfolder->width = 62;
   wo_newfolder->click_func = &add_folder;

   display_items();
   redraw();

   // main program loop
   while(1 == 1) {
      //for(int i = 0; i < (int)width; i++)
      //framebuffer[i] = 0;
      //asm volatile("pause");
      yield();
   }

   exit(0);

}