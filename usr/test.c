#include "prog.h"

#include "../lib/string.h"
#include "lib/stdio.h"
#include "lib/stdlib.h"

// tests and benchmarking for core OS functionality

void test_func(int func(void), char *name) {
   // todo: repeat n times, avg, max, min, variability, no fails
   // delta heap unused currently as stdlib allocation never free's

   // time function and print output
   uint32_t start_time = get_tick();
   //uint32_t start_heap = (uint32_t)sbrk(0);
   uint32_t start_mem = used_blocks();
   int result = func();
   uint32_t end_time = get_tick();
   //uint32_t end_heap = (uint32_t)sbrk(0);
   uint32_t end_mem = used_blocks();
   uint32_t delta_time = end_time - start_time;
   //uint32_t delta_heap = end_heap - start_heap;
   uint32_t delta_mem = end_mem - start_mem;

   printf("Test '%s' %s (result %i) after %u ms", name, result>=0?"passed":"failed!", result, delta_time);
   if(delta_mem) {
      printf(" - Leaked %u blocks!", delta_mem);
   }
   printf("\n");
}

#define SRC_FILE "/bmp/bg16.bmp"
#define SCRATCH_FILE "/tmp/scratch.bmp"

// stdio (vfs/fat/ata)

int test_fopen_file(void) {
   FILE *f = fopen(SRC_FILE, "r");
   if(!f)
      return -1;
   uint32_t s = fsize(fileno(f));
   uint8_t *buf = malloc(s);
   if(fread(buf, s, 1, f) <= 0) {
      fclose(f);
      free(buf);
      return -2;
   }
   fclose(f);
   free(buf);
   return 0;
}

int test_fwrite_file(void) {
   // overwrite
   int result = 0;
   FILE *f = fopen(SCRATCH_FILE, "r+");
   if(!f)
      return -1;
   uint32_t s = fsize(fileno(f));
   uint8_t *buf = malloc(s);
   if(fread(buf, s, 1, f) <= 0) {
      result = -2;
      goto cleanup;
   }
   if(fseek(f, 0, SEEK_SET) < 0) {
      result = -3;
      goto cleanup;
   }
   if(fwrite(buf, s, 1, f) != 1) {
      result = -4;
      goto cleanup;
   }

cleanup:
   fclose(f);
   free(buf);
   return result;
}

int test_fwrite_truncate_file(void) {
   // truncate & overwrite
   int result = 0;
   FILE *f = fopen(SCRATCH_FILE, "r+");
   if(!f)
      return -1;
   uint32_t s = fsize(fileno(f));
   uint8_t *buf = malloc(s);
   if(fread(buf, s, 1, f) <= 0) {
      result = -2;
      goto cleanup;
   }
   if(ftruncate(fileno(f), 0) < 0) {
      result = -3;
      goto cleanup;
   }
   if(fseek(f, 0, SEEK_SET) < 0) {
      result = -4;
      goto cleanup;
   }
   if(fwrite(buf, s, 1, f) != 1) {
      result = -5;
      goto cleanup;
   }

cleanup:
   fclose(f);
   free(buf);
   return result;
}

int test_fnew_file(void) {
   // create new file (duplicate from another)
   int result = 0;
   FILE *f = fopen(SRC_FILE, "r");
   if(!f)
      return -1;
   uint32_t s = fsize(fileno(f));
   uint8_t *buf = malloc(s);
   if(fread(buf, s, 1, f) <= 0) {
      result = -2;
      fclose(f);
      goto cleanup;
   }
   fclose(f);
   FILE *newf = fopen("/tmp/test.bmp", "w");
   if(!newf) {
      result = -3;
      goto cleanup;
   }
   if(fwrite(buf, s, 1, newf) != 1) {
      result = -4;
      goto cleanup2;
   }
cleanup2:
   fclose(newf);
   unlink("/tmp/test.bmp");
cleanup:
   free(buf);
   return result;
}

// copy SRC_FILE to SCRATCH_FILE (tmp file)
bool make_scratch(void) {
   FILE *src = fopen(SRC_FILE, "r");
   if(!src)
      return false;
   uint32_t s = fsize(fileno(src));
   uint8_t *buf = malloc(s);
   if(!buf) {
      fclose(src);
      return false;
   }
   bool ok = (fread(buf, s, 1, src) > 0);
   fclose(src);

   if(ok) {
      FILE *dst = fopen(SCRATCH_FILE, "w");
      if(!dst) {
         ok = false;
      } else {
         ok = (fwrite(buf, s, 1, dst) == 1);
         if(fclose(dst) != 0)
            ok = false;
      }
   }

   free(buf);
   return ok;
}

void _start() {

   int tmpfd = open("/tmp", FS_FLAG_READONLY);
   if(tmpfd < 0) {
      if(!mkdir("/tmp")) {
         printf("Couldn't create /tmp dir\n");
         exit(1);
      }
   } else {
      close(tmpfd);
   }

   if(!make_scratch()) {
      printf("Couldn't create scratch file " SCRATCH_FILE "\n");
      exit(1);
   }

   test_func(&test_fopen_file, "fopen_file");
   test_func(&test_fwrite_file, "fwrite_file");
   test_func(&test_fwrite_truncate_file, "fwrite_truncate_file");
   test_func(&test_fnew_file, "fnew_file");

   unlink(SCRATCH_FILE);

   while(true) { yield(); }

}