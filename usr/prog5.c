#include "prog.h"
#include "lib/wo_api.h"
#include "lib/stdio.h"

void click() {
   printf("Test 123\n");
   end_subroutine();
}

void _start(int argc, char **args) {
   int w = create_window(200, 140);
   printf("created window %i\n", w);
   int y = 50;
   windowobj_t *text = create_text_static_w(w, NULL, 0, y, "This is a popup window");
   text->click_func = (void*)&click;
   text->width = 200;
   text->texthalign = true;

   while(true) {
      yield();
   }
}