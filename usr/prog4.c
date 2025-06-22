#include <stdint.h>
#include <stdbool.h>

#include "prog.h"
#include "prog_bmp.h"
#include "prog_wo.h"

volatile uint8_t *image;

int x = 0;
int y = 0;

void timer_callback() {
    //clear();
    bmp_draw((uint8_t*)image, x%400, y%250, (x%2)+1, false);
    redraw();

    queue_event((uint32_t)(&timer_callback), 6);

    x+=5;
    y+=5;

    end_subroutine();
}

void click_callback() {
    clear();
    x = 0;
    timer_callback();
}

void strcpy(char* dest, char* src) {
   int i = 0;

   while(src[i] != '\0') {
      dest[i] = src[i];
      i++;
   }
   dest[i] = '\0';
}

void _start() {
    set_window_title("Prog4");
    override_draw((uint32_t)NULL);

    uint8_t *image = (uint8_t*)fat_read_file("/bmp/file20.bmp");
    if(image == NULL) {
        write_str("File icon not found\n");
        exit(0);
    }

    clear();

    int width = get_width();
    windowobj_t *wo = register_windowobj(WO_BUTTON, width - 65, 10, 50, 14);
    wo->text = (char*)malloc(1);
    strcpy(wo->text, "RESET");
    wo->click_func = &click_callback;

    // main program loop
   while(1 == 1) {
    asm volatile("nop");
   }
   exit(0);

}