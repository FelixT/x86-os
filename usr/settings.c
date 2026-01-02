#include "prog.h"
#include "../lib/string.h"
#include "lib/stdio.h"
#include "lib/dialogs.h"
#include "lib/ui/ui_mgr.h"
#include "lib/ui/ui_groupbox.h"
#include "lib/ui/ui_label.h"
#include "lib/ui/ui_input.h"
#include "lib/ui/ui_menu.h"
#include "lib/ui/ui_canvas.h"

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
   label_t *label_data = label->data;
   label_data->colour_border_light = 0xFFFF;
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

wo_t *colourpicker_wos[8]; // colourbox corresponding to each colourpicker dialog window

void settings_colourbox_callback(char *str, int w) {
   if(colourpicker_wos[w]) {
      for(int i = 0; i < ui->wo_count; i++) {
         wo_t *wo = ui->wos[i];
         if(wo->type != WO_GROUPBOX) continue;
         groupbox_t *groupbox = wo->data;
         canvas_t *canvas = groupbox->canvas->data;
         wo_t *prev_wo = ui->wos[i];
         for(int j = 0; j < canvas->child_count; j++) {
            if(canvas->children[j] == colourpicker_wos[w]) {
               // found colourbox, previous wo is input box
               label_t *colourbox = canvas->children[j]->data;
               colourbox->colour_bg = (uint16_t)hextouint(str+2);
               set_input_text(prev_wo, str);
               ui_draw(ui);
               input_t *input = prev_wo->data;
               input->return_func(prev_wo, -1);
               return;
            }
            
            prev_wo = canvas->children[j];
         }
      }
   }
}

void settings_colourbox_release(wo_t *wo) {
   label_t *label_data = wo->data;
   int d = dialog_colourpicker(label_data->colour_bg, &settings_colourbox_callback);
   colourpicker_wos[get_dialog(d)->window] = wo;
   debug_println("Dialog w %i", get_dialog(d)->window);
}

wo_t *settings_create_colourbox(wo_t *groupbox, int y, uint16_t colour) {
   wo_t *label = create_label(260, y, 20, 20, "");
   label_t *label_data = label->data;
   label_data->colour_bg = colour;
   label_data->filled = true;
   label_data->release_func = &settings_colourbox_release;
   groupbox_add(groupbox, label);
   return label;
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
   set_window_size(340, 280);

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
   wo_t *theme_group = create_groupbox(20, 10, 285, 155, "Theme settings");
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
   wo_t *input = settings_create_input(theme_group, y, buffer, &set_colour1);
   input->width-=20;
   settings_create_colourbox(theme_group, y, get_setting(SETTING_WIN_TITLEBARCOLOUR));
   y+=25;

   // theme colour 2
   settings_create_label(theme_group, y, "Colour 2");
   strcpy(buffer, "0x");
   uinttohexstr(get_setting(SETTING_WIN_TITLEBARCOLOUR2), buffer+2);
   input = settings_create_input(theme_group, y, buffer, &set_colour2);
   settings_create_colourbox(theme_group, y, get_setting(SETTING_WIN_TITLEBARCOLOUR2));
   input->width-=20;
   y+=25;

   // window background colour
   settings_create_label(theme_group, y, "Window background");
   strcpy(buffer, "0x");
   uinttohexstr(get_setting(SETTING_WIN_BGCOLOUR), buffer+2);
   input = settings_create_input(theme_group, y, buffer, &set_window_bgcolour);
   settings_create_colourbox(theme_group, y, get_setting(SETTING_WIN_BGCOLOUR));
   input->width-=20;
   y+=25;

   // window txt colour
   settings_create_label(theme_group, y, "Window text");
   strcpy(buffer, "0x");
   uinttohexstr(get_setting(SETTING_WIN_TXTCOLOUR), buffer+2);
   input = settings_create_input(theme_group, y, buffer, &set_window_txtcolour);
   settings_create_colourbox(theme_group, y, get_setting(SETTING_WIN_TXTCOLOUR));
   input->width-=20;
   y+=25;

   // font settings
   wo_t *font_group = create_groupbox(20, 180, 285, 65, "Font settings");
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
   wo_t *desktop_group = create_groupbox(20, 260, 285, 90, "Desktop settings");
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
   input = settings_create_input(desktop_group, y, buffer, &set_bgcolour);
   settings_create_colourbox(desktop_group, y, get_setting(SETTING_BGCOLOUR));
   input->width-=20;

   for(int i = 0; i < 8; i++)
      colourpicker_wos[i] = NULL;

   ui_draw(ui);

   create_scrollbar(&scroll);
   set_content_height(370);

   while(true) {
      yield();
   }
}