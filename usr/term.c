#include "prog.h"
#include <stdarg.h>
#include "../lib/string.h"
#include "lib/stdio.h"
#include "lib/stdlib.h"
#include "lib/sort.h"

char path[256] = "/";

void get_abs_path(char *out, char *inpath) {
   if(inpath[0] == '/') {
      // absolute path
      strcpy(out, inpath);
   } else {
      // relative path
      getwd(out);
      if(!strequ(out, "/"))
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
   printf(" CLEAR\n");
   printf(" LAUNCH<W> path, TASKS\n");
   printf(" FILES, TEXT <path>\n");
   printf(" FAPPEND <path> <buffer>\n");
   printf(" FWRITE <path> <buffer>\n");
   printf(" FREAD path\n");
   printf(" LS, CD path, PWD\n");
   printf(" CAT path, TOUCH path\n");
   printf(" MKDIR path, RENAME path newname\n");
   printf(" DMPMEM addr <bytes>\n");
   printf(" FONT path, RGBHEX r g b\n\n");
}

void term_cmd_clear() {
   clear();
   printf("User Terminal at %s\n", path);
}

void term_cmd_files() {
   launch_task("/sys/files.elf", 0, NULL, false, false);
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
      if(!strequ(buf, "/"))
         strcat(buf, "/");
      strcat(buf, arg);
   }
}

void term_cmd_font(char *arg) {
   set_setting(SETTING_SYS_FONT_PATH, (uint32_t)arg);
}

void term_cmd_viewbmp(char *arg) {
   char **args = (char**)malloc(sizeof(char*) * 1);
   args[0] = arg;
   launch_task("/sys/bmpview.elf", 1, args, false, false);
   free(args);
}

void term_cmd_fread(char *arg) {
   char path[256];
   get_abs_path(path, arg);
   FILE *f = fopen(path, 0);
   if(!f) {
      printf("File not found\n");
      return;
   }
   int size = fsize(fileno(f));
   char *buffer = malloc(size);
   int read = fread(buffer, size, 1, f);
   if(read)
      printf("File size %i loaded into 0x%h\n", size, (uint32_t)buffer);
   else
      printf("Couldn't read file\n");
}

void term_cmd_dmpmem(char *arg) {
   //arg1 = addr
   //arg2 = bytes

   int bytes = 32;
   int rowlen = 8;
   char arg2[10];

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
   free(buf);
}

void term_get_args(char *arg, char **args, int *argc) {
   char *p = arg;
   char temp[256];
   temp[0] = '\0';
   bool hit_limit = false;
   while(strsplit(temp, p, p, ' ')) {
      args[*argc] = (char*)malloc(strlen(temp) + 1);
      strcpy(args[*argc], temp);
      (*argc)++;
      if(*argc >= 10) {
         hit_limit = true;
         break;
      }
   }
   if(!hit_limit && temp[0] != '\0' && *argc < 10) {
      args[*argc] = (char*)malloc(strlen(temp) + 1);
      strcpy(args[*argc], temp);
      (*argc)++;
   }
}

void term_cmd_launch(char *arg, bool copy) {
   char patharg[256];
   char fullpath[256];
   char args[256];
   if(strsplit(patharg, args, arg, ' ')) {
      printf("Launching %s with args %s\n", patharg, args);
      // has args
      char **arglist = (char**)malloc(sizeof(char*) * 12);
      int argc = 0;
      // set path as first arg
      argtofullpath(fullpath, patharg);
      arglist[argc] = (char*)malloc(strlen(fullpath) + 1);
      strcpy(arglist[argc], fullpath);
      argc++;
      term_get_args(args, arglist, &argc);
      launch_task(fullpath, argc, arglist, copy, false);
      for(int i = 0; i < argc; i++)
         free(arglist[i]);
      free(arglist);
   } else {
      argtofullpath(fullpath, arg);
      char **arglist = (char**)malloc(sizeof(char*) * 1);
      arglist[0] = (char*)malloc(strlen(fullpath) + 1);
      strcpy(arglist[0], fullpath);
      launch_task(fullpath, 1, arglist, copy, false);
      free(arglist[0]);
      free(arglist);
   }
}

void term_cmd_text(char *arg) {
   char **args = (char**)malloc(sizeof(char*) * 1);
   args[0] = arg;
   launch_task("/sys/text.elf", 1, args, false, false);
   free(args);
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

int sort_dir(const void *v1, const void *v2) {
   const fs_dir_entry_t *d1 = (const fs_dir_entry_t*)v1;
   const fs_dir_entry_t *d2 = (const fs_dir_entry_t*)v2;

   return strcmp(d1->filename, d2->filename);
}

void term_cmd_ls(char *arg) {
   char *p = arg;
   if(strequ(p, ""))
      p = path;

   fs_dir_content_t *content = read_dir(p);
   if(content->entries)
      sort(content->entries, content->size, sizeof(fs_dir_entry_t), sort_dir);
   
   for(int i = 0; i < content->size; i++) {
      fs_dir_entry_t *entry = &content->entries[i];
      if(entry->hidden)
         continue;
      char filename_padded[16];
      memset(filename_padded, ' ', 15);
      memcpy(filename_padded, entry->filename, strlen(entry->filename));
      filename_padded[15] = '\0';
      printf(" %s", filename_padded);
      if(entry->type == FS_TYPE_DIR) {
         printf(" DIR");
      } else {
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
         printf(" %i %s", size, type);
      }
      printf("\n");
   }

   kfree(content->entries, sizeof(fs_dir_entry_t) * content->size);
   kfree(content, sizeof(fs_dir_content_t));
}

void term_cmd_cd(char *arg) {
   if(strequ(arg, "..") || strequ(arg, "../")) {
      // go up one directory
      int len = strlen(path);
      for(int i = len-1; i >= 0; i--) {
         if(path[i] == '/') {
            path[i] = '\0';
            break;
         }
      }
   } else if(strequ(arg, ".") || arg[0] == '\0' || strequ(arg, "./")) {
      // do nothing
   } else if(arg[0] == '/') {
      // absolute path
      strcpy(path, arg);
   } else{
      if(!strequ(path, "/"))
         strcat(path, "/");
      strcat(path, arg);
   }
   if(strequ(path, "")) {
      strcat(path, "/");
   }
   chdir(path);
   printf("Changed directory to %s\n", path);
}

void term_cmd_touch(char *arg) {
   char path[256];

   get_abs_path(path, arg);

   int fd = new_file(path);
   if(fd < 0) {
      printf("Failed to create file '%s'\n", path);
   } else {
      close(fd);
   }
}

void term_cmd_pwd() {
   char *wd = (char*)malloc(256);
   getwd(wd);
   printf("%s\n", wd);
   free(wd);
   return;
}

void term_cmd_cat(char *arg) {
   char path[256];
   get_abs_path(path, arg);
   FILE *f = fopen(path, "r");
   if(!f) {
      printf("File %s not found\n", path);
      return;
   }
   int size = fsize(fileno(f));
   printf("Opened file '%s' with size %u\n", path, size);
   char *buf = (char*)malloc(size+1);
   if(!fread(buf, size, 1, f)) {
      printf("Couldn't read file\n");
      free(buf);
      return;
   }
   buf[f->content_size] = '\0';
   printf("%s\n", buf);
   free(buf);
   fclose(f);
   printf("Closed file\n");
}

void term_cmd_fappend(char *arg) {
   char path[256];
   char patharg[256];
   char buffer[256];
   strsplit(patharg, buffer, arg, ' ');
   get_abs_path(path, patharg);
   
   FILE *f = fopen(path, "a");
   if(!f) {
      printf("File '%s' not found\n", path);
      return;
   }
   int size = fsize(fileno(f));
   printf("Opened file '%s' with size %u\n", path, size);
   fwrite(buffer, strlen(buffer), 1, f);
   fclose(f);
   printf("Closed file\n");
}

void term_cmd_fwrite(char *arg) {
   char path[256];
   char patharg[256];
   char buffer[256];
   strsplit(patharg, buffer, arg, ' ');
   get_abs_path(path, patharg);
   
   FILE *f = fopen(path, "w");
   int size = fsize(fileno(f));
   printf("Opened file '%s' with size %u\n", path, size);
   int c = fwrite(buffer, strlen(buffer), 1, f);
   printf("Wrote %u bytes\n", c);
   fclose(f);
   printf("Closed file\n");
}

void term_cmd_mkdir(char *arg) {
   char path[256];
   get_abs_path(path, arg);

   if(!mkdir(path)) {
      printf("Failed to create directory '%s'\n", path);
   }
}

void term_cmd_rename(char *arg) {
   char path[256];
   char newname[40];

   if(!strsplit(path, newname, arg, ' ')) {
      printf("Usage: RENAME path newname\n");
      return;
   }

   char fullpath[256];
   get_abs_path(fullpath, path);

   if(!rename(fullpath, newname)) {
      printf("Renaming '%s' to '%s' failed\n", fullpath, newname);
   }
}

void term_cmd_tasks() {
   tasks_t tasks = get_tasks();
   for(int i = 0; i < tasks.size; i++) {
      api_task_t *task = &tasks.tasks[i];
      if(!task->enabled) continue;
      printf("\nTask %i: ", task->id);
      if(task->parentid == task->id)
         printf("%s (allc %i pages / heap size 0x%h)", task->main_window_name, task->no_allocated, task->heap_end - task->heap_start);
      else
         printf("(parent %i) ", task->parentid);
      if(task->paused)
         printf("paused");
   }
   printf("\n");
}

void term_pipe(char *arg1, char *arg2) {
   bool arg1_args = strchr(arg1, ' ') != NULL;
   bool arg2_args = strchr(arg2, ' ') != NULL;

   char **args1 = (char**)malloc(sizeof(char*) * 10);
   char **args2 = (char**)malloc(sizeof(char*) * 10);
   int argc1 = 1;
   int argc2 = 1;
   char path[256];
   char path1[256];
   char path2[256];

   if(arg1_args) {
      strsplit(path, arg1, arg1, ' ');
      get_abs_path(path1, path);
      term_get_args(arg1, args1, &argc1);
   } else {
      get_abs_path(path1, arg1);
   }

   args1[0] = (char*)malloc(strlen(path1) + 1);
   strcpy(args1[0], path1);

   if(arg2_args) {
      strsplit(path, arg2, arg2, ' ');
      get_abs_path(path2, path);
      term_get_args(arg2, args2, &argc2);
   } else {
      get_abs_path(path2, arg2);
   }

   args2[0] = (char*)malloc(strlen(path2) + 1);
   strcpy(args2[0], path2);

   int fds[2];
   pipe(&fds[0], &fds[1]);  // create pipe first

   int saved_stdout = dup(1);
   dup2(fds[1], 1); // redirect stdout to pipe write end
   int prog1_id = launch_task(path1, argc1, args1, true, true);
   for(int i = 0; i < argc1; i++) free(args1[i]);
   free(args1);
   if(prog1_id < 0) {
      printf("Failed to launch %s\n", path1);
      for(int i = 0; i < argc2; i++) free(args2[i]);
      free(args2);
      return;
   }
   // restore stdout if exists, otherwise close fd 1 created by dup2 so child doesn't inherit it
   if(saved_stdout >= 0) {
      dup2(saved_stdout, 1);
      close(saved_stdout);
   } else {
      close(1);
   }
   close(fds[1]); // parent no longer needs write end, child has its own copy

   int saved_stdin = dup(0);
   dup2(fds[0], 0); // redirect stdin to pipe read end
   int prog2_id = launch_task(path2, argc2, args2, true, true);
   for(int i = 0; i < argc2; i++) free(args2[i]);
   free(args2);
   if(prog2_id < 0) {
      printf("Failed to launch %s\n", path2);
      return;
   }
   // restore stdin
   if(saved_stdin >= 0) {
      dup2(saved_stdin, 0);
      close(saved_stdin);
   } else {
      close(0);
   }
   close(fds[0]);

   unpause_task(prog1_id);
   unpause_task(prog2_id);
}

void checkcmd(char *buffer) {

   bool args = strchr(buffer, ' ') != NULL;
   bool pipe = strchr(buffer, '|') != NULL;

   char command[128];
   char arg[128];
   if(pipe) {
      strsplit(command, arg, buffer, '|');
      term_pipe(command, arg);
      kfree(buffer, 1); // should be programs responsibility to free
      end_subroutine();
      return;
   } else if(args) {
      strsplit(command, arg, buffer, ' ');
   } else {
      strcpy(command, buffer);
      arg[0] = '\0';
   }
   strtoupper(command);

   if(strequ(command, "HELP"))
      term_cmd_help();
   else if(strequ(command, "CLEAR"))
      term_cmd_clear();
   else if(strequ(command, "FREAD"))
      term_cmd_fread(arg);
   else if(strequ(command, "FILES")) 
      term_cmd_files();
   else if(strequ(command, "FONT"))
      term_cmd_font(arg);
   else if(strequ(command, "VIEWBMP"))
      term_cmd_viewbmp(arg);
   else if(strequ(command, "DMPMEM"))
      term_cmd_dmpmem(arg);
   else if(strequ(command, "LAUNCH"))
      term_cmd_launch(arg, true);
   else if(strequ(command, "LAUNCHW"))
      term_cmd_launch(arg, false);
   else if(strequ(command, "TEXT"))
      term_cmd_text(arg);
   else if(strequ(command, "RGBHEX"))
      term_cmd_rgbhex(arg);
   else if(strequ(command, "LS"))
      term_cmd_ls(arg);
   else if(strequ(command, "CD"))
      term_cmd_cd(arg);
   else if(strequ(command, "TOUCH"))
      term_cmd_touch(arg);
   else if(strequ(command, "PWD"))
      term_cmd_pwd();
   else if(strequ(command, "CAT"))
      term_cmd_cat(arg);
   else if(strequ(command, "FAPPEND"))
      term_cmd_fappend(arg);
   else if(strequ(command, "FWRITE"))
      term_cmd_fwrite(arg);
   else if(strequ(command, "MKDIR"))
      term_cmd_mkdir(arg);
   else if(strequ(command, "RENAME"))
      term_cmd_rename(arg);
   else if(strequ(command, "TASKS"))
      term_cmd_tasks();
   else
      term_cmd_default(command);

   kfree(buffer, 1); // should be programs responsibility to free

   end_subroutine();
}

void _start(int argc, char **args) {
   set_window_title("User Terminal");

   char *wd = (char*)malloc(256);
   getwd(wd);
   strcpy(path, wd);
   free(wd);

   printf("User Terminal at %s\n", path);

   if(argc > 1) {
      checkcmd(args[1]);
      exit(0);
   }

   override_term_checkcmd(&checkcmd);

   while(true) {
      yield();
   }
}
