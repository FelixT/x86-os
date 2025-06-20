#include "prog.h"

#include "../lib/string.h"

void printf(char *format, ...) {
   char *buffer = (char*)malloc(512);
   va_list args;
   va_start(args, format);
   vsnprintf(buffer, 512, format, args);
   va_end(args);
   write_str(buffer);
   free((uint32_t)buffer, 512);
}

void _start() {
   write_str("Calc\n");
   override_term_checkcmd((uint32_t)NULL);
   char *buf = (char*)malloc(100);
   while(true) {
      read(buf);
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