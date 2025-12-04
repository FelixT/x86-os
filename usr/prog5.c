#include "prog.h"
#include "lib/stdio.h"
#include "lib/dialogs.h"

void callback(char *str) {
   debug_println("Yes %s", str);
}

void _start() {
   set_window_title("Prog5");

   int dialog = dialog_input("Test123", (void*)&callback);
   printf("created dialog %i\n", dialog);
   
   while(true) {
      yield();
   }
}