#include "prog.h"
#include <stdarg.h>
#include "../lib/string.h"
#include "prog_fat.h"

void printf(char *format, ...) {
   char *buffer = (char*)malloc(512);
   va_list args;
   va_start(args, format);
   vsprintf(buffer, format, args);
   va_end(args);
   write_str(buffer);
   free((uint32_t)buffer, 512);
}

void term_cmd_default(char *command) {
   printf("Command not found: %s\n", command);
}

void term_cmd_help() {
   // todo: mouse, tasks, prog1, prog2, files, viewbmp, test, desktop, mem, bg, bgimg, padding, redrawall, windowbg, windowtxt
   write_str("\n");
   printf("CLEAR\n");
   printf("LAUNCH path\n");
   printf("FAT path\n");
   printf("FATDIR cluster, FATFILE cluster\n");
   printf("DMPMEM x <y>\n");
   printf("FONT path\n");
}

void term_cmd_clear() {
   clear();
   printf("Terminal emulator\n");
}

void term_cmd_files() {
   launch_task("/sys/files.elf", 0, NULL);
}

uint32_t argtouint(char *str) {
   uint32_t num = 0;
   if(strstartswith(str, "0x")) {
      str+=2;
      while(*str != '\0') {
         if(*str >= '0' && *str <= '9')
            num = (num << 4) + (*str - '0');
         else if(*str >= 'A' && *str <= 'F')
            num = (num << 4) + (*str - 'A' + 10);
         else if(*str >= 'a' && *str <= 'f')
            num = (num << 4) + (*str - 'a' + 10);
         str++;
      }
   } else {
      while(*str != '\0') {
         if(*str >= '0' && *str <= '9')
            num = (num * 10) + (*str - '0');
         str++;
      }
   }
   return num;
}

void printdir(fat_dir_t *items, int size) {
   for(int i = 0; i < size; i++) {
      // print dir entry
      if(items[i].filename[0] == 0) continue;
      if(items[i].firstClusterNo < 2) continue;
      if((items[i].attributes & 0x02) == 0x02) continue; // hidden

      char fileName[9];
      char extension[4];
      strcpy_fixed((char*)fileName, (char*)items[i].filename, 8);
      strcpy_fixed((char*)extension, (char*)items[i].filename+8, 3);
      strsplit((char*)fileName, NULL, (char*)fileName, ' '); // null terminate at first space
      strsplit((char*)extension, NULL, (char*)extension, ' '); // null terminate at first space
      printf("%s%s%s:",fileName,(extension[0] != '\0') ? "." : "", extension);
      if((items[i].attributes & 0x10) == 0x10) // directory
         printf("DIR");
      else
         printf("%u",items[i].fileSize);
      
      printf(" <%i>\n", items[i].firstClusterNo);

   }
}

void term_cmd_fat(char *arg) {
   fat_dir_t *items = NULL;
   int size = 0;
   if(strcmp(arg, "/")) {
      // root
      printf("Root directory\n");
      items = (fat_dir_t*)fat_read_root();
      fat_bpb_t *fat_bpb = (fat_bpb_t*)fat_get_bpb();
      size = fat_bpb->noRootEntries;
      free((uint32_t)fat_bpb, sizeof(fat_bpb_t));
      printdir(items, size);
      free((uint32_t)items, sizeof(fat_dir_t) * fat_bpb->noRootEntries);
   } else {

      fat_dir_t *entry = (fat_dir_t*)fat_parse_path(arg, true);
      if(entry == NULL) {
         printf("Path not found\n", 0);
         free((uint32_t)entry, sizeof(fat_dir_t));
         return;
      }
      if(entry->attributes & 0x10) {
         printf("Directory %s found in cluster %u\n", arg, entry->firstClusterNo);
         int size = fat_get_dir_size((uint16_t) entry->firstClusterNo);
         fat_dir_t *items = (fat_dir_t*)fat_read_dir(entry->firstClusterNo);
         printdir(items, size);
         fat_bpb_t *fat_bpb = (fat_bpb_t*)fat_get_bpb();
         free((uint32_t)items, fat_bpb->sectorsPerCluster * fat_bpb->bytesPerSector);
         free((uint32_t)fat_bpb, sizeof(fat_bpb_t));
      } else {
         printf("File %s found in cluster %u\n", arg, entry->firstClusterNo);
      }
      free((uint32_t)entry, sizeof(fat_dir_t));

   }
}

void term_cmd_font(char *arg) {
   set_sys_font(arg);
}

void term_cmd_viewbmp(char *arg) {
   int argc = 1;
   char **args = NULL;
   args = (char**)malloc(sizeof(char*) * 1);
   char *path = (char*)malloc(strlen(arg));
   strcpy(path, arg);
   args[0] = path;
   launch_task("/sys/bmpview.elf", argc, args);
}

void term_cmd_fatfile(char *arg) {
   int cluster = stoi(arg);
   uint8_t *file = (uint8_t*)fat_read_file((uint16_t)cluster, 0);
   printf("File loaded into %u / 0x%h\n", (uint32_t)file, (uint32_t)file);
}

void term_cmd_fatdir(char *arg) {
   int cluster = stoi(arg);
   int size = fat_get_dir_size((uint16_t) cluster);

   fat_dir_t *items = (fat_dir_t*)fat_read_dir((uint16_t)cluster);
   printdir(items, size);

   //free((uint32_t)items, 32 * size);
}

void term_cmd_dmpmem(char *arg) {
   //arg1 = addr
   //arg2 = bytes

   int bytes = 32;
   int rowlen = 8;
   char arg2[10];
   printf("%s\n", arg);

   if(strsplit(arg, arg2, arg, ' ')) {
      bytes = stoi((char*)arg2);
   }
   uint32_t addr = argtouint((char*)arg);
   printf("%i bytes at %u\n", bytes, addr);

   char *buf = (char*)malloc(rowlen);
   buf[rowlen] = '\0';
   uint8_t *mem = (uint8_t*)addr;
   for(int i = 0; i < bytes; i++) {
      if(mem[i] <= 0x0F)
         printf("0");
      printf("%h ", mem[i]);
      buf[i%rowlen] = mem[i];

      if((i%rowlen) == (rowlen-1) || i==(bytes-1)) {
         for(int x = 0; x < rowlen; x++) {
            if(buf[x] != '\n')
               printf("%c", buf[x]);
         }
         printf("\n");
      }
   }
   free((uint32_t)buf, rowlen);
}

void term_cmd_fatnew(char *arg) {
   (void)arg;
   printf("TODO\n");
}

void term_cmd_launch(char *arg) {
   launch_task(arg, 0, NULL);
}

void checkcmd(char *buffer) {

   char command[10];
   char arg[30];
   strsplit((char*)command, (char*)arg, buffer, ' '); // super unsafe
   strtoupper(command, command);

   if(strcmp(command, "HELP"))
      term_cmd_help();
   else if(strcmp(command, "CLEAR"))
      term_cmd_clear();
   else if(strcmp(command, "FAT"))
      term_cmd_fat(arg);
   else if(strcmp(command, "FATDIR"))
      term_cmd_fatdir(arg);
   else if(strcmp(command, "FATFILE"))
      term_cmd_fatfile(arg);
   else if(strcmp(command, "FATNEW"))
      term_cmd_fatnew(arg);
   else if(strcmp(command, "FILES"))
      term_cmd_files();
   else if(strcmp(command, "FONT"))
      term_cmd_font(arg);
   else if(strcmp(command, "VIEWBMP"))
      term_cmd_viewbmp(arg);
   else if(strcmp(command, "DMPMEM"))
      term_cmd_dmpmem(arg);
   else if(strcmp(command, "LAUNCH"))
      term_cmd_launch(arg);

   else
      term_cmd_default(command);

   free((uint32_t)buffer, 1); // should be programs responsibility to free

   end_subroutine();
}

void _start() {
      // clear the screen
      clear();

      write_str("UsrTerm\n");

      override_term_checkcmd((uint32_t)(&checkcmd));

      while(true) {
         yield();
      }
}
