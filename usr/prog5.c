#include "prog.h"
#include "lib/wo_api.h"
#include "lib/stdio.h"
#include "lib/dialogs.h"

void click() {
   printf("Test 123\n");
   end_subroutine();
}

void callback(char *str) {
   debug_println("Yes %s", str);
}

void _start() {
   int w = dialog_input("Test123", (void*)&callback);
   printf("created window %i\n", w);

   while(true) {
      yield();
   }
}