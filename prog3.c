#include <stdint.h>

#include "prog.h"

void main() {
    write_num(123);
    
    uint16_t *buf = (uint16_t*)get_framebuffer();

    for(int i = 0; i < 1000; i++)
      buf[i] = 0;

    redraw();

    exit(1);
}