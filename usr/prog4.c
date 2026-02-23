#include <stdint.h>
#include <stdbool.h>

#include "prog.h"
#include "prog_bmp.h"
#include "lib/stdio.h"
#include "lib/stdlib.h"
#include "../lib/string.h"
#include "lib/dialogs.h"

uint8_t *image;

int x = 0;
int y = 0;

dialog_t *dialog;

void timer_callback() {
    //clear();
    bmp_draw((uint8_t*)image, x%400, y%250, (x%2)+1, false);
    ui_draw(dialog->ui);
    redraw();

    queue_event(&timer_callback, 6, NULL);

    x+=5;
    y+=5;

    end_subroutine();
}

void click_callback(wo_t *wo, int window) {
    (void)wo;
    (void)window;
    clear();
    queue_event(&timer_callback, 6, NULL);
}

void _start() {
    dialog = get_dialog(get_free_dialog());
    dialog_init(dialog, -1);
    dialog_set_title(dialog, "Prog4");

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
    wo_t *wo = create_button(width - 65, 10, 50, 14, "Reset");
    set_button_release(wo, &click_callback);
    ui_add(dialog->ui, wo);

    ui_draw(dialog->ui);

    // main program loop
   while(1 == 1) {
    asm volatile("nop");
   }
   exit(0);

}