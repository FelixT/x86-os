#include <stdint.h>
#include <stdbool.h>

#include "prog.h"
#include "prog_bmp.h"

volatile uint8_t *image;

int x = 0;
int y = 0;

void timer_callback() {
    //clear();
    bmp_draw((uint8_t*)image, x%400, y%250, (x%2)+1);
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
    write_str("Prog4\n");
    override_draw((uint32_t)NULL);

    fat_dir_t *entry = (fat_dir_t*)fat_parse_path("/bmp/file20.bmp", true);
    if(entry == NULL) {
        write_str("File icon not found\n");
        exit(0);
    }
    image = (uint8_t*)fat_read_file(entry->firstClusterNo, entry->fileSize);

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