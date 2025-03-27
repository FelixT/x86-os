#include <stdint.h>
#include <stdbool.h>

#include "prog.h"

typedef struct {
   uint8_t start[3];
   uint8_t identifier[8];
   uint16_t bytesPerSector;
   uint8_t sectorsPerCluster;
   uint16_t noReservedSectors;
   uint8_t noTables;
   uint16_t noRootEntries;
   uint16_t noSectors;
   uint8_t mediaDescriptor;
   uint16_t sectorsPerFat;
   uint16_t sectorsPerTrack;
   uint16_t noDriveHeads;
   uint32_t noHiddenSectors;
   uint32_t largeNoSectors; // used if value doesn't fit noSectors

} __attribute__((packed)) fat_bpb_t;

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

int strlen(char* str) {
   int len = 0;
   while(str[len] != '\0')
      len++;
   return len;
}

bool strcmp(char* str1, char* str2) {
   int len = strlen(str1);
   if(len != strlen(str2))
      return false;

   for(int i = 0; i < len; i++)
      if(str1[i] != str2[i])
         return false;

   return true;
}

void display_items() {
   int y = 5;
   int x = 5;

   clear(0xFFFF); // white
   
   int offsetLeft = offset;
   int position = 0;

   for(int i = 0; i < no_items; i++) {
      if(cur_items[i].filename[0] == 0) break;

      bool hidden = (cur_items[i].attributes & 0x02) == 0x02;
      bool dotentry = cur_items[i].filename[0] == 0x2E;

      if(hidden & !dotentry) continue; // ignore
      
      offsetLeft--;
      if(offsetLeft >= 0) continue;

      cur_items[i].zero = position;

      char fileName[9];
      char extension[4];

      for(int x = 0; x < 8; x++)
         fileName[x] = cur_items[i].filename[x];
      fileName[8] = '\0';

      for(int x = 0; x < 3; x++)
         extension[x] = cur_items[i].filename[x+8];
      extension[3] = '\0';

      // draw
      if((cur_items[i].attributes & 0x10) == 0x10) {
         // directory
         bmp_draw((uint8_t*)folder_icon, x, y);
         write_strat(fileName, x + 25, y + 4);
      } else {
         // file
         bmp_draw((uint8_t*)file_icon, x, y);
         write_strat(fileName, x + 25, y + 4);

         if(extension[0] != ' ') {
            write_strat(extension, x + 105, y + 4);
            write_strat(extension, x + 105, y + 4);

            write_numat(cur_items[i].fileSize, x + 150, y + 4);
         }
      }


      y += 25;
      position++;
   }

   // draw path
   write_strat(cur_path, 0, 280);
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

   // see where we clicked
   int position = (y-5)/25;
   //debug_write_uint(no_items);

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
      if(cur_items[index].firstClusterNo == 0) {
         read_root();
      } else {
         // update path
         int pi = strlen(cur_path);
         if(pi == 1) pi = 0;
         cur_path[pi] = '/';
         int x;
         for(x = 0; x < 8 && cur_items[index].filename[x] != ' '; x++)
            cur_path[pi+x+1] = cur_items[index].filename[x];
         cur_path[pi+x+1] = '\0';

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


      // cur_items[index].firstClusterNo;
   }

   display_items();
   redraw();

   end_subroutine();

}

void resize(uint32_t fb, uint32_t w, uint32_t h) {
   framebuffer = (uint16_t*)fb;
   width = w;
   height = h;

   display_items();
   redraw();
   end_subroutine();
}

void _start() {

   // init
   cur_items = NULL;
   no_items = 0;
   offset = 0;
   fat_dir_t *entry = (fat_dir_t*)fat_parse_path("/bmp/file20.bmp");
   if(entry == NULL) {
      write_str("File icon not found\n");
      exit(0);
   }
   file_icon = (uint8_t*)fat_read_file(entry->firstClusterNo, entry->fileSize);

   entry = (fat_dir_t*)fat_parse_path("/bmp/folder20.bmp");
   if(entry == NULL) {
      write_str("Folder icon not found\n");
      exit(0);
   }
   folder_icon = (uint8_t*)fat_read_file(entry->firstClusterNo, entry->fileSize);

   framebuffer = (uint16_t*)get_framebuffer();

   override_uparrow((uint32_t)&uparrow);
   override_downarrow((uint32_t)&downarrow);
   override_click((uint32_t)&click);
   override_draw((uint32_t)NULL);
   override_resize((uint32_t)&resize);
   width = get_width();
   height = get_height();

   read_root();
   display_items();
   redraw();

   // main program loop
   while(1 == 1) {
      for(int i = 0; i < (int)width; i++)
      framebuffer[i] = 0;
   }

   exit(0);

}