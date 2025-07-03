#include "prog.h"
#include <stdarg.h>
#include "../lib/string.h"
#include "prog_fat.h"
#include "lib/stdio.h"

char path[256] = "/";

void printf(char *format, ...) {
   char *buffer = (char*)malloc(512);
   va_list args;
   va_start(args, format);
   vsnprintf(buffer, 512, format, args);
   va_end(args);
   write_str(buffer);
   free((uint32_t)buffer, 512);
}

void get_abs_path(char *out, char *inpath) {
   if(inpath[0] == '/') {
      // absolute path
      strcpy(out, inpath);
   } else {
      // relative path
      getwd(out);
      if(!strcmp(out, "/"))
         strcat(out, "/");
      strcat(out, inpath);
   }
}

void term_cmd_default(char *command) {
   printf("Command not found: %s\n", command);
}

void term_cmd_help() {
   // todo: mouse, tasks, prog1, prog2, files, viewbmp, test, desktop, mem, bg, bgimg, padding, redrawall, windowbg, windowtxt
   write_str("\n");
   printf("CLEAR\n");
   printf("LAUNCH path\n");
   printf("FILES, TEXT <path>\n");
   printf("FAT <path>, FATNEW path\n");
   printf("FATDIR cluster, FREAD path\n");
   printf("LS, CD path, PWD\n");
   printf("CAT path, TOUCH path, MKDIR path\n");
   printf("DMPMEM x <y>\n");
   printf("FONT path\n");
}

void term_cmd_clear() {
   clear();
   printf("User Terminal at %s\n", path);
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

void argtofullpath(char *buf, char *arg) {
   if(arg[0] == '/') {
      strcpy(buf, arg);
   } else {
      buf[0] = '\0';
      strcat(buf, path);
      if(!strcmp(buf, "/"))
         strcat(buf, "/");
      strcat(buf, arg);
   }
}

void tolower(char *c) {
   for(int i = 0; i < strlen(c); i++) {
      if(c[i] >= 'A' && c[i] <= 'Z')
         c[i] += ('a'-'A');
   }
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
      tolower((char*)fileName);
      tolower((char*)extension);
      strsplit((char*)fileName, NULL, (char*)fileName, ' '); // null terminate at first space
      strsplit((char*)extension, NULL, (char*)extension, ' '); // null terminate at first space
      printf(" %s%s%s ", fileName, (extension[0] != '\0') ? "." : "", extension);
      if((items[i].attributes & 0x10) == 0x10) // directory
         printf("DIR\n");
      else {
         // file
         uint32_t size = items[i].fileSize;
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
         printf("<%u %s>\n", size, type);
         
      }
   }
}

void term_cmd_fat(char *arg) {
   fat_dir_t *items = NULL;
   int size = 0;
   if(strcmp(arg, "/") || strcmp(arg, "")) {
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
   char **args = (char**)malloc(sizeof(char*) * 1);
   args[0] = arg;
   launch_task("/sys/bmpview.elf", 1, args);
}

void term_cmd_fread(char *arg) {
   char path[256];
   get_abs_path(path, arg);
   uint8_t *file = (uint8_t*)fat_read_file(path);
   printf("File loaded into 0x%h\n", (uint32_t)file);
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
   fat_new_file(arg);
}

void term_cmd_launch(char *arg) {
   char fullpath[256];
   argtofullpath(fullpath, arg);
   launch_task(fullpath, 0, NULL);
}

void term_cmd_text(char *arg) {
   char **args = (char**)malloc(sizeof(char*) * 1);
   args[0] = arg;
   launch_task("/sys/text.elf", 1, args);
}

void term_cmd_rgbhex(char *arg) {
   char first[5];
   char second[5];
   char third[5];
   strsplit(first, arg, arg, ' ');
   strsplit(second, third, arg, ' ');
   int r = stoi(first);
   int g = stoi(second);
   int b = stoi(third);
   int n = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
   printf("0x%h\n", n);
}

void term_cmd_ls(char *arg) {
   char *p = arg;
   if(strcmp(p, ""))
      p = path;

   fat_dir_t *items = NULL;
   int size = 0;
   if(strcmp(p, "/") || strcmp(p, "")) {
      // root
      items = (fat_dir_t*)fat_read_root();
      fat_bpb_t *fat_bpb = (fat_bpb_t*)fat_get_bpb();
      size = fat_bpb->noRootEntries;
      free((uint32_t)fat_bpb, sizeof(fat_bpb_t));
      printdir(items, size);
      free((uint32_t)items, sizeof(fat_dir_t) * fat_bpb->noRootEntries);
   } else {

      fat_dir_t *entry = (fat_dir_t*)fat_parse_path(p, true);
      if(entry == NULL)
         return;
      if(entry->attributes & 0x10) {
         int size = fat_get_dir_size((uint16_t) entry->firstClusterNo);
         fat_dir_t *items = (fat_dir_t*)fat_read_dir(entry->firstClusterNo);
         printdir(items, size);
         fat_bpb_t *fat_bpb = (fat_bpb_t*)fat_get_bpb();
         free((uint32_t)items, fat_bpb->sectorsPerCluster * fat_bpb->bytesPerSector);
         free((uint32_t)fat_bpb, sizeof(fat_bpb_t));
      }
      free((uint32_t)entry, sizeof(fat_dir_t));

   }
}

void term_cmd_cd(char *arg) {
   if(strcmp(arg, "..") || strcmp(arg, "../")) {
      // go up one directory
      int len = strlen(path);
      for(int i = len-1; i >= 0; i--) {
         if(path[i] == '/') {
            path[i] = '\0';
            break;
         }
      }
   } else if(strcmp(arg, ".") || arg[0] == '\0' || strcmp(arg, "./")) {
      // do nothing
   } else if(arg[0] == '/') {
      // absolute path
      strcpy(path, arg);
   } else{
      if(!strcmp(path, "/"))
         strcat(path, "/");
      strcat(path, arg);
   }
   if(strcmp(path, "")) {
      strcat(path, "/");
   }
   chdir(path);
   printf("Changed directory to %s\n", path);
}

void term_cmd_touch(char *arg) {
   char path[256];

   get_abs_path(path, arg);

   fat_new_file(path);
}

void term_cmd_pwd() {
   char *wd = (char*)malloc(256);
   getwd(wd);
   printf("%s\n", wd);
   free((uint32_t)wd, 256);
   return;
}

void term_cmd_cat(char *arg) {
   char path[256];
   get_abs_path(path, arg);
   FILE *f = fopen(path, "r");
   if(!f) {
      printf("File %s not found\n");
      return;
   }
   printf("Opened file '%s' with size %u\n", path, f->size);
   char *buf = (char*)malloc(f->size+1);
   if(!fread(buf, f->size, 1, f)) {
      printf("Couldn't read file\n");
      return;
   }
   buf[f->content_size] = '\0';
   printf("%s\n", buf);
   free((uint32_t)buf, f->size);
   fclose(f);
   free((uint32_t)f, sizeof(FILE));
   printf("Closed file\n");
}

void term_cmd_fappend(char *arg) {
   char path[256];
   char patharg[256];
   char buffer[256];
   strsplit(patharg, buffer, arg, ' ');
   get_abs_path(path, patharg);
   
   FILE *f = fopen(path, "a");
   printf("Opened file '%s' with size %u\n", path, f->size);
   fwrite(buffer, strlen(buffer), 1, f);
   fclose(f);
   free((uint32_t)f, sizeof(FILE));
   printf("Closed file\n");
}

void term_cmd_fwrite(char *arg) {
   char path[256];
   char patharg[256];
   char buffer[256];
   strsplit(patharg, buffer, arg, ' ');
   get_abs_path(path, patharg);
   
   FILE *f = fopen(path, "w");
   printf("Opened file '%s' with size %u\n", path, f->size);
   fwrite(buffer, strlen(buffer), 1, f);
   printf("Wrote %u bytes\n", f->content_size);
   fclose(f);
   free((uint32_t)f, sizeof(FILE));
   printf("Closed file\n");
}

void term_cmd_mkdir(char *arg) {
   char path[256];
   get_abs_path(path, arg);

   fat_new_dir(path);
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
   else if(strcmp(command, "FREAD"))
      term_cmd_fread(arg);
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
   else if(strcmp(command, "LAUNCH"))
      term_cmd_launch(arg);
      else if(strcmp(command, "LAUNCH"))
      term_cmd_launch(arg);
   else if(strcmp(command, "TEXT"))
      term_cmd_text(arg);
   else if(strcmp(command, "RGBHEX"))
      term_cmd_rgbhex(arg);
   else if(strcmp(command, "LS"))
      term_cmd_ls(arg);
   else if(strcmp(command, "CD"))
      term_cmd_cd(arg);
   else if(strcmp(command, "TOUCH"))
      term_cmd_touch(arg);
   else if(strcmp(command, "PWD"))
      term_cmd_pwd();
   else if(strcmp(command, "CAT"))
      term_cmd_cat(arg);
   else if(strcmp(command, "FAPPEND"))
      term_cmd_fappend(arg);
   else if(strcmp(command, "FWRITE"))
      term_cmd_fwrite(arg);
   else if(strcmp(command, "MKDIR"))
      term_cmd_mkdir(arg);
   else
      term_cmd_default(command);

   free((uint32_t)buffer, 1); // should be programs responsibility to free

   end_subroutine();
}

void _start() {
      set_window_title("User Terminal");

      char *wd = (char*)malloc(256);
      getwd(wd);
      strcpy(path, wd);
      free((uint32_t)wd, 256);

      printf("User Terminal at %s\n", path);

      override_term_checkcmd((uint32_t)(&checkcmd));

      while(true) {
         yield();
      }
}
