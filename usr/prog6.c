#include "lib/ui/ui_button.h"
#include "lib/ui/ui_label.h"
#include "lib/ui/ui_input.h"
#include "lib/ui/ui_menu.h"
#include "lib/ui/ui_mgr.h"
#include "lib/ui/ui_grid.h"
#include "lib/ui/ui_groupbox.h"
#include "lib/ui/ui_canvas.h"
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
   if(!ui)
      end_subroutine();
      
   ui_keypress(ui, c);
   end_subroutine();
}

void resize() {
   s = get_surface();
   ui->surface = &s;
   ui_draw(ui);
   end_subroutine();
}

void hover(int x, int y) {
   if(!ui)
      end_subroutine();

   ui_hover(ui, x, y);
   end_subroutine();
}

wo_t *menu = NULL;

void toggle_menu(wo_t *wo, int window) {
   (void)wo;
   (void)window;
   clear();
   menu->visible = !menu->visible;
   ui_draw(ui);
}

void click_grid(int row, int col) {
   debug_println("Clicked %i %i", row, col);
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

   wo_t *togglemenu = create_button(170, 95, 80, 20, "Menu");
   ((button_t*)togglemenu->data)->release_func = &toggle_menu;
   ui_add(ui, togglemenu);

   menu = create_menu(10, 95, 150, 65);
   ui_add(ui, menu);
   add_menu_item(menu, "Item 1", NULL);
   add_menu_item(menu, "Item 2", NULL);

   wo_t *grid = create_grid(10, 175, 100, 100, 2, 2);
   grid_t *grid_data = (grid_t*)grid->data;
   wo_t *label2 = create_label(5, 5, 20, 20, "xx");
   wo_t *input2 = create_input(5, 5, 30, 20);
   grid_add(grid, label2, 0, 0);
   grid_add(grid, input2, 1, 0);
   grid_data->click_func = &click_grid;
   ui_add(ui, grid);

   wo_t *groupbox = create_groupbox(120, 175, 100, 100, "Test");
   wo_t *input3 = create_input(5, 5, 30, 20);
   groupbox_add(groupbox, input3);
   ui_add(ui, groupbox);

   wo_t *canvas = create_canvas(230, 175, 100, 100);
   wo_t *label3 = create_label(5, 5, 40, 20, "Canvas");
   canvas_add(canvas, label3);
   ui_add(ui, canvas);

   // draw
   ui_draw(ui);

   while(true) {
      yield();
   }
}