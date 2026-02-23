#include "lib/ui/ui_button.h"
#include "lib/ui/ui_label.h"
#include "lib/ui/ui_input.h"
#include "lib/ui/ui_menu.h"
#include "lib/ui/ui_mgr.h"
#include "lib/ui/ui_grid.h"
#include "lib/ui/ui_groupbox.h"
#include "lib/ui/ui_canvas.h"
#include "lib/ui/ui_textarea.h"
#include "lib/stdio.h"
#include "lib/draw.h"

// test program for UI library

ui_mgr_t *ui;
surface_t s;

void drawbg() {
   draw_checkeredrect(&s, 0xBDF7, 0xDEDB, 0, 0, s.width, s.height);
}

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
   drawbg();
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
   drawbg();
   menu->visible = !menu->visible;
   ui_draw(ui);
}

int click_grid(wo_t *wo, int window, int row, int col) {
   (void)wo;
   (void)window;
   debug_println("Clicked %i %i", row, col);
   return 0;
}

void rightclick(int x, int y, int window) {
   (void)window;
   ui_rightclick(ui, x, y);
   end_subroutine();
}

void scroll(int deltaY, int offsetY, int window) {
   (void)window;
   ui_scroll(ui, deltaY, offsetY);
   drawbg();
   ui_draw(ui);
   redraw();
   end_subroutine(deltaY, offsetY);
}

void _start() {

   set_window_title("Prog6");

   // init ui
   s = get_surface();
   ui = ui_init(&s, -1);
   override_draw(0, -1);
   override_click(&click, -1);
   override_release(&release, -1);
   override_keypress(&keypress, -1);
   override_resize(&resize, -1);
   override_hover(&hover, -1);
   override_rightclick(&rightclick, -1);

   create_scrollbar(&scroll, -1);
   set_content_height(420, -1);

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
   set_button_release(togglemenu, &toggle_menu);
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
   wo_t *label4 = create_label(50, 50, 70, 70, "Overflowing label");
   canvas_add(canvas, label3);
   canvas_add(canvas, label4);
   ui_add(ui, canvas);

   wo_t *textarea = create_textarea(5, 300, 100, 100);
   ui_add(ui, textarea);

   // draw
   drawbg();
   ui_draw(ui);

   // setup rightclick menu
   ui->default_menu = create_menu(0, 0, 120, 75);
   ui->default_menu->visible = false;
   add_menu_item(ui->default_menu, "Default 1", NULL);
   add_menu_item(ui->default_menu, "Default 2", NULL);
   ui_add(ui, ui->default_menu);

   while(true) {
      yield();
   }
}