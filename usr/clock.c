#include "prog.h"

#include "../lib/string.h"

#include "lib/ui/ui_mgr.h"
#include "lib/ui/ui_label.h"

surface_t s;
ui_mgr_t *ui;

void resize() {
   s = get_surface();
   ui->surface = &s;
   ui_draw(ui);
   end_subroutine();
}

void _start() {
   set_window_size(160, 40);
   set_window_position(get_surface_w(-2).width - 160 - 10, 10, -1);
   set_window_title("Clock");

   s = get_surface();
   ui = ui_init(&s, -1);
   override_draw(0, -1);
   override_resize(&resize, -1);

   wo_t *hour_label = create_label(10, 10, 40, 14, "HH");
   get_label(hour_label)->filled = true;
   ui_add(ui, hour_label);

   wo_t *colon_label1 = create_label(50, 12, 10, 14, ":");
   get_label(colon_label1)->bordered = false;
   ui_add(ui, colon_label1);

   wo_t *minute_label = create_label(60, 10, 40, 14, "MM");
   get_label(minute_label)->filled = true;
   ui_add(ui, minute_label);

   wo_t *colon_label2 = create_label(100, 12, 10, 14, ":");
   get_label(colon_label2)->bordered = false;
   ui_add(ui, colon_label2);

   wo_t *second_label = create_label(110, 10, 40, 14, "SS");
   get_label(second_label)->filled = true;
   ui_add(ui, second_label);

   uint32_t last_seconds = -1;

   while(true) {
      uint32_t time = get_time();
      uint32_t hours = (time / 3600) % 24;
      uint32_t minutes = (time / 60) % 60;
      uint32_t seconds = time % 60;
      if(seconds == last_seconds) continue;
      last_seconds = seconds;

      bool leading = hours < 10;
      if(leading) get_label(hour_label)->label[0] = '0';
      uinttostr(hours, get_label(hour_label)->label + leading);
      leading = minutes < 10;
      if(leading) get_label(minute_label)->label[0] = '0';
      uinttostr(minutes, get_label(minute_label)->label + leading);
      leading = seconds < 10;
      if(leading) get_label(second_label)->label[0] = '0';
      uinttostr(seconds, get_label(second_label)->label + leading);

      ui_draw(ui);
      redraw();
      sleep(200);
   }
}