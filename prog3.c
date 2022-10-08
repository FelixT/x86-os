#include <stdint.h>

#include "prog.h"

uint32_t num;

void test(int windowIndex) {

  write_num(windowIndex);
  write_newline();

  num = 4;

  for(int i = 0; i < 10; i++)
  //while(1==1)
    write_num(num);
    
  write_newline();

  end_subroutine();

}

void _start() {
  num = 9;

  asm volatile(
    "int $0x30"
    :: "a" (11),
    "b" ((uint32_t)&test)
  );
    
  uint16_t *buf = (uint16_t*)get_framebuffer();

  for(int i = 0; i < 1000; i++)
    buf[i] = 0;

  write_newline();
  redraw();

  while(1 == 1) {
      write_num(num);
  }

  exit(0);

}