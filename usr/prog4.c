#include <stdint.h>
#include <stdbool.h>

#include "prog.h"
#include "prog_bmp.h"
#include "prog_wo.h"
#include "lib/stdio.h"
#include "../lib/string.h"

uint8_t *image;

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

void _start() {
    set_window_title("Prog4");
    override_draw((uint32_t)NULL);

    FILE *f = fopen("/bmp/file20.bmp", "r");
    if(!f) {
        write_str("File icon not found\n");
        exit(0);
    }

    int size = fsize(fileno(f));
    if(size <= 0) {
        write_str("Invalid file\n");
    }
    image = malloc(size);
    int read = fread(image, size, 1, f);

    if(read <= 0) {
        write_str("Couldn't read file\n");
        exit(0);
    }

    clear();

    int width = get_width();
    windowobj_t *wo = register_windowobj(-1, WO_BUTTON, width - 65, 10, 50, 14);
    wo->text = (char*)malloc(1);
    strcpy(wo->text, "RESET");
    wo->click_func = &click_callback;

    // main program loop
   while(1 == 1) {
    asm volatile("nop");
   }
   exit(0);

}