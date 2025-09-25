#include <stdint.h>
#include <stdbool.h>
#include "../lib/string.h"
#include "lib/wo_api.h"


#include "prog.h"
#include "prog_fat.h"

volatile uint16_t *framebuffer;
volatile uint32_t width;
volatile uint32_t height;
volatile uint8_t *file_icon;
volatile uint8_t *folder_icon;
volatile fat_bpb_t *bpb;
volatile fat_dir_t *cur_items;
volatile int no_items;
volatile int offset;

char cur_path[200];

char tolower(char c) {
   if(c >= 'A' && c <= 'Z')
      c += ('a'-'A');
   return c;
}

void display_items() {
   int y = 5;
   int x = 5;

   clear();
   
   int offsetLeft = offset;
   int position = 0;

   for(int i = 0; i < no_items; i++) {
      if(cur_items[i].filename[0] == 0) break;

      bool hidden = (cur_items[i].attributes & 0x02) == 0x02;
      bool dotentry = cur_items[i].filename[0] == 0x2E;

      if(hidden & !dotentry) {
         cur_items[i].zero = -1;
         continue; // ignore
      }
      
      offsetLeft--;
      if(offsetLeft >= 0) {
         cur_items[i].zero = -1;
         continue;
      }

      cur_items[i].zero = position;

      char fileName[9];
      char extension[4];
      char fullName[13];

      for(int j = 0; j < 8; j++)
         fileName[j] = tolower(cur_items[i].filename[j]);
      fileName[8] = '\0';

      for(int j = 0; j < 3; j++)
         extension[j] = tolower(cur_items[i].filename[j+8]);
      extension[3] = '\0';

      strsplit(fullName, NULL, fileName, ' ');
      if(extension[0] != ' ') {
         strcat(fullName, ".");
         strcat(fullName, extension);
      }

      // draw
      if((cur_items[i].attributes & 0x10) == 0x10) {
         // directory
         bmp_draw((uint8_t*)folder_icon, x, y, 1, true);
         write_strat(fileName, x + 27, y + 7);
      } else {
         // file
         bmp_draw((uint8_t*)file_icon, x, y, 1, true);
         write_strat(fullName, x + 27, y + 7);

         uint32_t size = cur_items[i].fileSize;
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

   // draw path
   for(int y = (int)height - 15; y < (int)height; y++) {
      for(int x = 0; x < (int)width; x++) {
         framebuffer[y*width+x] = 0xFFFF;
      }
   }
   write_strat(cur_path, 5, height - 12);
}

void read_root() {
   cur_path[0] = '/';
   cur_path[1] = '\0';
   // read root
   bpb = (fat_bpb_t*) fat_get_bpb();
   cur_items = (fat_dir_t*)fat_read_root();
   no_items = bpb->noRootEntries;
   // get real root size
   for(int i = 0; i < bpb->noRootEntries; i++) {
      if(cur_items[i].filename[0] == 0) {
         no_items = i;
         break;
      }
   }
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
   if(offset >= no_items) offset = no_items - 1;

   display_items();
   redraw();

   end_subroutine();

}

void click(int x, int y) {

   (void)(x);
   if(x > (int)width - 20)
      end_subroutine();

   // see where we clicked
   int position = (y-5)/25;

   if(cur_items == NULL) end_subroutine();

   int index = -1;
   // find where .zero (position) == position
   for(int i = 0; i < no_items; i++) {
      if(cur_items[i].zero == position)
         index = i;
   }

   if(index < 0) end_subroutine();

   if((cur_items[index].attributes & 0x10) == 0x10) {
      // directory
      // free((uint32_t)cur_items, no_items*32);
      offset = 0;
      if(cur_items[index].firstClusterNo == 0) {
         read_root();
      } else {
         // update path
         if(cur_items[index].filename[0] == '.') {
            if(cur_items[index].filename[1] == '.') {
               int lastslashpos = -1;
               for(int i = 0; i < strlen(cur_path); i++) {
                  if(cur_path[i] == '/')
                     lastslashpos = i;
               }
               if(lastslashpos)
                  cur_path[lastslashpos] = '\0';
            } else {
               // do nothing
            }
         } else {
            int pi = strlen(cur_path);
            if(pi == 1) pi = 0;
            cur_path[pi] = '/';
            int x;
            for(x = 0; x < 8 && cur_items[index].filename[x] != ' '; x++)
               cur_path[pi+x+1] = tolower(cur_items[index].filename[x]);
            cur_path[pi+x+1] = '\0';
         }

         // read dir
         no_items = fat_get_dir_size(cur_items[index].firstClusterNo);
         cur_items = (fat_dir_t*)fat_read_dir(cur_items[index].firstClusterNo);
      }

      // cur_items[index].firstClusterNo;
   } else {
      // file

      int x;

      char extension[4];
      for(x = 0; x < 3; x++)
         extension[x] = cur_items[index].filename[x+8];
      extension[3] = '\0';

      char fullpath[200];
      int pi = strlen(cur_path);
      for(x = 0; x < pi; x++)
         fullpath[x] = cur_path[x];
      fullpath[pi] = '/';
      for(x = 0; x < 8 && cur_items[index].filename[x] != ' '; x++)
         fullpath[pi+x+1] = cur_items[index].filename[x];
      pi+=x+1;
      fullpath[pi] = '.';
      for(x = 0; x < 4; x++)
         fullpath[pi+x+1] = extension[x];

      // handle supported extensions
      // bmp, elf, txt
      if(strcmp(extension, "BMP")) {
         
         char **args = (char**)malloc(1);
         args[0] = fullpath;

         // note: this also ends the subroutine
         launch_task("/sys/bmpview.elf", 1, args);
      }

      if(strcmp(extension, "ELF")) {
         // note: this also ends the subroutine
         launch_task(fullpath, 0, NULL);
      }

      if(strcmp(extension, "TXT")) {
         
         char **args = (char**)malloc(1);
         args[0] = fullpath;

         // note: this also ends the subroutine
         launch_task("/sys/text.elf", 1, args);
      }

      if(strcmp(extension, "FON")) {
         set_sys_font(fullpath);
      }


      // cur_items[index].firstClusterNo;
   }

   display_items();
   redraw();

   end_subroutine();

}

void resize(uint32_t fb, uint32_t w, uint32_t h) {
   (void)w;
   framebuffer = (uint16_t*)fb;
   width = get_surface().width;
   height = h;

   display_items();
   redraw();
   end_subroutine();
}

void scroll(int deltaY, int offsetY) {
   (void)deltaY;
   offset = (offsetY + 24) / 25;
   if(offset >= no_items) offset = no_items - 1;
   display_items();
   redraw();
   end_subroutine();
}

void _start(int argc, char **args) {
   (void)argc;
   (void)args;

   // init
   set_window_title("File Manager");

   cur_items = NULL;
   no_items = 0;
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

   read_root();
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