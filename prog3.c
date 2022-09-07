#include <stdint.h>

void main() {
    /*// output 123
    asm(
        "int $0x30"
        // put operands in eax, ebx, etcs
        :: "a" (6),
        "b" (123), // print 123
      );
    }*/

    static volatile uint32_t framebuffer;
    // get framebuffer
    asm volatile (
      "int $0x30;movl %%ecx, %0;"
      : "=r" (framebuffer) // output
      : "a" (7) // input
    );

    //for(int i = 0; i < 10000; i++)
    //  ((uint16_t*) 1568256)[i] = 0;

    volatile uint16_t *buf;
    buf = (uint16_t*)framebuffer;

    for(int i = 0; i < 1000; i++)
      buf[i] = 0;

    // draw
    asm volatile(
      "int $0x30"
      :: "a" (9)
    );

    // end task with status 0
    asm volatile(
    "int $0x30"
    :: "a" (10),
    "b" (0)
  );
}