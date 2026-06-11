#include "prog.h"

volatile uint32_t flag = 0;

void thread() {
   while(true) {
      write_str("Second thread\n");
      flag = 1;
      futex_wake((void*)&flag);
      sleep(1000);
   }
}

void thread2() {
   while(true) {
      // wait for thread1
      while(flag == 0)
        futex_wait((void*)&flag, 0);
      write_str("Third thread\n");
      flag = 0;
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