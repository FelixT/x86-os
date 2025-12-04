#include "lib/ui/ui_button.h"
#include "lib/ui/ui_label.h"
#include "lib/ui/ui_input.h"
#include "lib/ui/ui_mgr.h"
#include "lib/stdio.h"

// test program for UI library

ui_mgr_t *ui;
surface_t s;

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

void keypress(uint16_t c, int window) {
   (void)window;
   ui_keypress(ui, c);
   end_subroutine();
}

void resize() {
   s = get_surface();
   ui->surface = &s;
   end_subroutine();
}

void hover(int x, int y) {
   ui_hover(ui, x, y);
   end_subroutine();
}

void _start() {

   set_window_title("Prog6");

   // init ui
   s = get_surface();
   ui = ui_init(&s, -1);
   override_draw(0);
   override_click((uint32_t)&click, -1);
   override_release((uint32_t)&release, -1);
   override_keypress((uint32_t)&keypress, -1);
   override_resize((uint32_t)&resize);
   override_hover((uint32_t)&hover, -1);

   // create & register elements
   wo_t *btn = create_button(10, 40, 150, 20, "Test");
   btn->click_func = &btn_click;
   btn->release_func = &btn_release;
   ui_add(ui, btn);

   wo_t *label = create_label(10, 10, 150, 20, "Hello, World\ntest");
   ui_add(ui, label);

   wo_t *input = create_input(10, 70, 150, 20);
   ui_add(ui, input);

   // draw
   ui_draw(ui);

   while(true) {
      yield();
   }
}