#include "lib/ui/ui_button.h"
#include "lib/ui/ui_label.h"
#include "lib/ui/ui_mgr.h"
#include "lib/stdio.h"

// test program for UI library

ui_mgr_t *ui;

void click(int x, int y, int window) {
   (void)window;
   if(!ui)
      end_subroutine();

   ui_click(ui, x, y);

   end_subroutine();
}

void release(int x, int y, int window) {
   (void)window;
   if(!ui)
      end_subroutine();

   ui_release(ui, x, y);

   end_subroutine();
}

void btn_click() {
   debug_println("Button clicked");
}

void btn_release() {
   debug_println("Button released");
}

void _start() {

   // init ui
   surface_t s = get_surface();
   ui = ui_init(&s);
   override_draw(0);
   override_click((uint32_t)&click);
   override_release((uint32_t)&release);

   // create & register elements
   wo_t *btn = create_button(10, 40, 150, 20, "Test");
   btn->click_func = &btn_click;
   btn->release_func = &btn_release;
   ui_add(ui, btn);

   wo_t *label = create_label(10, 10, 150, 20, "Hello, World");
   label_t *label_data = (label_t *)label->data;
   ui_add(ui, label);

   // draw
   ui_draw(ui);

   while(true) {
      yield();
   }
}