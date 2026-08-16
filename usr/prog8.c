#include "prog.h"
#include "lib/stdio.h"
#include "lib/stdlib.h"
#include "../lib/string.h"

// ipc shared memory test program

typedef struct shared_mem_t {
   uint32_t flag; // futex flag for sync
} __attribute__((packed)) shared_mem_t;

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

      shared_mem_t *shared_obj = (shared_mem_t*)buf;
      while(shared_obj->flag == 0) {
         futex_wait((void*)&shared_obj->flag, 0);
      }

      uint8_t *bmp = buf + sizeof(shared_mem_t);
      bmp_draw(bmp, 0, 0, 1, false);
      redraw();
      exit(0);
   }

   // parent
   set_window_title("prog8 parent");
   override_draw(NULL, -1);
   
   // create shared memory
   char *bmp_path = "/bmp/bg2.bmp";
   int bmp_size = fpsize(bmp_path);
   if(bmp_size <= 0) {
      printf("fpsize failed\n");
      exit(0);
   }
   int shared_size = sizeof(shared_mem_t) + bmp_size;
   shared_t shared = shared_create(shared_size);
   if(!shared.mem) {
      printf("shared_create failed\n");
      exit(0);
   }

   shared_mem_t *shared_obj = (shared_mem_t*)shared.mem;
   shared_obj->flag = 0;

   // launch child with block uid as 2nd param
   char uidstr[16]; 
   inttostr(shared.uid, uidstr);
   char *cargs[2] = { "child", uidstr };

   int task = launch_task("/sys/prog8.elf", 2, cargs, false, true);
   shared_grant(task, shared.uid);
   unpause_task(task);

   // open bmp file and read into shared memory
   FILE *f = fopen(bmp_path, "r");
   if(!f) {
      printf("couldn't open file\n");
      exit(0);
   }

   void *bmp_ptr = shared.mem + sizeof(shared_mem_t);

   if(!fread(bmp_ptr, bmp_size, 1, f)) {
      printf("read failed\n");
      exit(0);
   }
   fclose(f);

   shared_obj->flag = 1; // signal child to draw
   futex_wake((void*)&shared_obj->flag);

   bmp_draw((uint8_t*)bmp_ptr, 0, 0, 1, false);
   redraw();

   exit(0);
}
