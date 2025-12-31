#include "prog.h"

void thread() {
   while(true) {
      write_str("Second thread\n");
   }
}

void _start() {
   set_window_title("Prog7");
   create_thread(&thread);
   while(true) {
      write_str("Main thread\n");
      yield();
   }
}