#include "prog.h"

int delta = 10;

void timer_callback() {
    write_str("Subroutine hit\n");

    queue_event(&timer_callback, delta, NULL);

    delta += 10;

    end_subroutine();
}

void _start() {
    set_window_title("Prog3");

    queue_event(&timer_callback, 50, NULL);
    queue_event(&timer_callback, 100, NULL);

    write_str("Test123\n");


    // main program loop
   while(1 == 1) {
    asm volatile("nop");
   }
   exit(0);

}