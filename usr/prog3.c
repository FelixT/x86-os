#include "prog.h"

int delta = 10;

void timer_callback() {
    write_str("Subroutine hit\n");

    queue_event((uint32_t)(&timer_callback), delta);

    delta += 10;

    end_subroutine();
}

void _start() {
    set_window_title("Prog3");

    queue_event((uint32_t)(&timer_callback), 50);
    queue_event((uint32_t)(&timer_callback), 100);

    write_str("Test123\n");


    // main program loop
   while(1 == 1) {
    asm volatile("nop");
   }
   exit(0);

}