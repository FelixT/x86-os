#include "prog.h"

#include "../lib/string.h"

void printf(char *format, ...) {
   char *buffer = (char*)malloc(512);
   va_list args;
   va_start(args, format);
   vsnprintf(buffer, 512, format, args);
   va_end(args);
   write(0, buffer, 512);
   free(buffer, 512);
}

void _start() {
   printf("Calc\n");
   override_term_checkcmd((uint32_t)NULL);
   uint32_t start = (uint32_t)sbrk(0);
   char *buf = (char*)sbrk(0x4000);
   //sbrk(-0x3000);
   uint32_t size = (uint32_t)sbrk(0) - start;
   printf("Size 0x%h\n", size);

   while(true) {
      read(0, buf, size);
      printf("Read string: %s\n", buf);
      int number;
      if(strstartswith(buf, "0x")) {
         number = hextouint(buf);
      } else {
         number = strtoint(buf);
      }
      printf("int: %i\nhex: %h\n", number, number);
   }

   exit(0);
}