#include "prog.h"
#include "../lib/string.h"
#include "lib/dialogs.h"
#include "lib/ui/ui_mgr.h"
#include "lib/ui/ui_groupbox.h"
#include "lib/ui/ui_label.h"
#include "lib/ui/ui_input.h"
#include "lib/ui/ui_menu.h"

surface_t s;
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

void hover(int x, int y) {
   if(!ui)
      end_subroutine();

   ui_hover(ui, x, y);
   end_subroutine();
}

void resize() {
   if(!ui)
      end_subroutine();

   s = get_surface();
   ui->surface = &s;
   ui_draw(ui);
   end_subroutine();
}

void keypress(uint16_t c, int window) {
   (void)window;
   if(!ui)
      end_subroutine();

   ui_keypress(ui, c);
   end_subroutine();
}

wo_t *settings_create_label(wo_t *groupbox, int y, char *text) {
   wo_t *label = create_label(5, y, 130, 20, text);
   groupbox_add(groupbox, label);
   return label;
}

wo_t *settings_create_input(wo_t *groupbox, int y, char *text, void (*callback)(wo_t *wo, int window)) {
   wo_t *input = create_input(140, y, 140, 20);
   input_t *input_data = input->data;
   input_data->valign = true;
   input_data->halign = true;
   input_data->return_func = callback;
   strcpy(input_data->text, text);
   input_data->cursor_pos = strlen(input_data->text);
   groupbox_add(groupbox, input);
   return input;
}

void scroll(int deltaY, int offsetY) {
   (void)offsetY;

   clear();

   for(int i = 0; i < ui->wo_count; i++) {
      wo_t *wo = ui->wos[i];
      if(!wo || !wo->enabled) continue;
      ui->wos[i]->y -= deltaY;
   }

   ui_draw(ui);

   end_subroutine();
}

void set_theme(wo_t *wo, int index, int window) {
   (void)wo;
   (void)window;
   set_setting(SETTING_THEME_TYPE, index);
}

void set_colour1(wo_t *wo, int window) {
   (void)window;
   input_t *input = wo->data;
   set_setting(SETTING_WIN_TITLEBARCOLOUR, hextouint(input->text + 2));
}

void set_colour2(wo_t *wo, int window) {
   (void)window;
   input_t *input = wo->data;
   set_setting(SETTING_WIN_TITLEBARCOLOUR2, hextouint(input->text + 2));
}

void set_window_bgcolour(wo_t *wo, int window) {
   (void)window;
   input_t *input = wo->data;
   set_setting(SETTING_WIN_BGCOLOUR, hextouint(input->text + 2));
}

void set_window_txtcolour(wo_t *wo, int window) {
   (void)window;
   input_t *input = wo->data;
   set_setting(SETTING_WIN_TXTCOLOUR, hextouint(input->text + 2));
}

void set_font_padding(wo_t *wo, int window) {
   (void)window;
   input_t *input = wo->data;
   set_setting(SETTINGS_SYS_FONT_PADDING, strtoint(input->text));
}

void set_bgcolour(wo_t *wo, int window) {
   (void)window;
   input_t *input = wo->data;
   set_setting(SETTING_BGCOLOUR, hextouint(input->text + 2));
}

void set_desktop_enabled(wo_t *wo, int window) {
   (void)window;
   input_t *input = wo->data;
   set_setting(SETTING_DESKTOP_ENABLED, strtoint(input->text));
}

void set_bgimage(wo_t *wo, int window) {
   (void)window;
   input_t *input = wo->data;
   if(!set_setting(SETTING_DESKTOP_BGIMG_PATH, (uint32_t)input->text))
      dialog_msg("Error", "Couldn't set background image");
}

void set_font(wo_t *wo, int window) {
   (void)window;
   input_t *input = wo->data;
   if(!set_setting(SETTING_SYS_FONT_PATH, (uint32_t)input->text))
      dialog_msg("Error", "Couldn't set font");
}

void _start() {
   set_window_title("Settings");

   // init ui
   s = get_surface();
   ui = ui_init(&s, -1);
   override_draw(0);
   override_click((uint32_t)&click, -1);
   override_release((uint32_t)&release, -1);
   override_resize((uint32_t)&resize);
   override_hover((uint32_t)&hover, -1);
   override_keypress((uint32_t)&keypress, -1);

   char buffer[256];

   // theme settings
   wo_t *theme_group = create_groupbox(10, 10, 285, 155, "Theme settings");
   ui_add(ui, theme_group);
   int y = 5;

   // theme type
   wo_t *label = settings_create_label(theme_group, y, "Theme");
   wo_t *menu = create_menu(140, y, 140, 35);
   menu_t *menu_data = menu->data;
   add_menu_item(menu, "Classic", &set_theme);
   add_menu_item(menu, "Gradient", &set_theme);
   menu_data->selected_index = get_setting(SETTING_THEME_TYPE);
   groupbox_add(theme_group, menu);
   label->height = 35;
   y+=40;

   // theme colour 1
   settings_create_label(theme_group, y, "Colour 1");
   strcpy(buffer, "0x");
   uinttohexstr(get_setting(SETTING_WIN_TITLEBARCOLOUR), buffer+2);
   settings_create_input(theme_group, y, buffer, &set_colour1);
   y+=25;

   // theme colour 2
   settings_create_label(theme_group, y, "Colour 2");
   strcpy(buffer, "0x");
   uinttohexstr(get_setting(SETTING_WIN_TITLEBARCOLOUR2), buffer+2);
   settings_create_input(theme_group, y, buffer, &set_colour2);
   y+=25;

   // window background colour
   settings_create_label(theme_group, y, "Window background");
   strcpy(buffer, "0x");
   uinttohexstr(get_setting(SETTING_WIN_BGCOLOUR), buffer+2);
   settings_create_input(theme_group, y, buffer, &set_window_bgcolour);
   y+=25;

   // window txt colour
   settings_create_label(theme_group, y, "Window text");
   strcpy(buffer, "0x");
   uinttohexstr(get_setting(SETTING_WIN_TXTCOLOUR), buffer+2);
   settings_create_input(theme_group, y, buffer, &set_window_txtcolour);
   y+=25;

   // font settings
   wo_t *font_group = create_groupbox(10, 180, 285, 65, "Font settings");
   ui_add(ui, font_group);
   y = 5;

   // font
   settings_create_label(font_group, y, "Font");
   strcpy(buffer, (char*)get_setting(SETTING_SYS_FONT_PATH));
   settings_create_input(font_group, y, buffer, &set_font);
   y+=25;

   // font padding
   settings_create_label(font_group, y, "Padding");
   inttostr(get_setting(SETTINGS_SYS_FONT_PADDING), buffer);
   settings_create_input(font_group, y, buffer, &set_font_padding);
   y+=25;

   // desktop settings
   wo_t *desktop_group = create_groupbox(10, 260, 285, 95, "Desktop settings");
   ui_add(ui, desktop_group);
   y = 5;

   // desktop enabled
   settings_create_label(desktop_group, y, "Desktop enabled");
   inttostr(get_setting(SETTING_DESKTOP_ENABLED), buffer);
   settings_create_input(desktop_group, y, buffer, &set_desktop_enabled);
   y+=25;

   // desktop bgimg
   settings_create_label(desktop_group, y, "Background image");
   strcpy(buffer, (char*)get_setting(SETTING_DESKTOP_BGIMG_PATH));
   settings_create_input(desktop_group, y, buffer, &set_bgimage);
   y+=25;

   // desktop bg colour
   settings_create_label(desktop_group, y, "Background colour");
   strcpy(buffer, "0x");
   uinttohexstr(get_setting(SETTING_BGCOLOUR), buffer+2);
   settings_create_input(desktop_group, y, buffer, &set_bgcolour);

   ui_draw(ui);

   create_scrollbar(&scroll);
   set_content_height(370);

   while(true) {
      yield();
   }
}