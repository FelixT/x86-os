#include "prog.h"

void thread() {
   while(true) {
      write_str("Second thread\n");
      sleep(1000);
   }
}

void thread2() {
   while(true) {
      write_str("Third thread\n");
      sleep(1000);
   }
}

void _start() {
   set_window_title("Prog7");
   create_thread(&thread);
   create_thread(&thread2);
   while(true) {
      write_str("Main thread\n");
      sleep(1000);
   }
}