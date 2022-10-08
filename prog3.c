#include <stdint.h>

#include "prog.h"

volatile uint16_t *framebuffer;
volatile uint32_t width;
volatile uint32_t height;

void uparrow(int windowIndex) {

  write_uint(width);
  write_newline();

  write_uint(height);
  write_newline();

  end_subroutine();

}

void click(int windowIndex, int x, int y) {

  if(y >= 0) {
    framebuffer[y*width+x] = 0;
  } // otherwise we clicked on the titlebar

  end_subroutine();

}

void _start() {
  // init
  //width = malloc();
  //height = malloc();

  framebuffer = (uint16_t*)get_framebuffer();

  override_uparrow((uint32_t)&uparrow);
  override_click((uint32_t)&click);
  width = get_width();
  height = get_height();
  
  // main program loop
  while(1 == 1) {
    for(int i = 0; i < width; i++)
      framebuffer[i] = 0;
  }

  exit(0);

}