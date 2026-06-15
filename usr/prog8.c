#include "prog.h"
#include "lib/stdio.h"
#include "lib/stdlib.h"
#include "../lib/string.h"

// ipc shared memory test program

void _start(int argc, char **args) {
   if(argc > 0 && strequ(args[0], "child")) {
      // child
      set_window_title("prog8 child");
      override_draw(NULL, -1);
      uint32_t uid = (uint32_t)strtoint(args[1]);
      uint8_t *buf = (uint8_t*)shared_map(uid);
      if(!buf) {
         printf("shared_map failed\n");
         exit(0);
      }

      bmp_draw(buf, 0, 0, 1, false);
      redraw();
      exit(0);
   }

   // parent
   set_window_title("prog8 parent");
   override_draw(NULL, -1);
   FILE *f = fopen("/bmp/bg2.bmp", "r");
   if(!f) {
      printf("Couldn't open file\n");
      exit(0);
   }
   int size = fsize(fileno(f));

   shared_t shared = shared_create(size);
   if(!shared.mem) {
      printf("shared_create failed\n");
      exit(0);
   }

   if(!fread(shared.mem, size, 1, f)) { printf("read failed\n"); exit(0); }
   fclose(f);
   uint8_t *pbuf = (uint8_t*)shared.mem;
   bmp_draw(pbuf, 0, 0, 1, false);
   redraw();

   // launch child with block uid as 2nd param
   char uidstr[16]; 
   inttostr(shared.uid, uidstr);
   char *cargs[2] = { "child", uidstr };

   int task = launch_task("/sys/prog8.elf", 2, cargs, false, true);
   shared_grant(task, shared.uid);
   unpause_task(task);

   exit(0);
}
