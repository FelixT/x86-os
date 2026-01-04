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
#include "lib/ui/ui_checkbox.h"
#include "lib/draw.h"

surface_t s;
ui_mgr_t *ui;
wo_t *bgimg_input;
wo_t *fontpath_input;
wo_t *bgcolour_input;
wo_t *fontpadding_input;
wo_t *desktopenabled_input;
wo_t *bgcolour_colourbox;
wo_t *colour1_colourbox;
wo_t *colour2_colourbox;
wo_t *windowbg_colourbox;
wo_t *windowtxt_colourbox;

int scrollOffsetY = 0;

void title_draw(wo_t *wo, surface_t *surface, int window, int offset, int offsetY) {
   write_strat_w("System settings:", 10, 8-scrollOffsetY, 0, -1);
   draw_line(surface, rgb16(200, 200, 200), 10, 18-scrollOffsetY, false, strlen("System settings")*(get_font_info().width+get_font_info().padding));
}

wo_t *settings_create_label(wo_t *groupbox, int y, char *text) {
   wo_t *label = create_label(5, y, 130, 20, text);
   label_t *label_data = label->data;
   label_data->colour_border_light = 0xFFFF;
   label_data->colour_border_dark = rgb16(220, 220, 220);
   label_data->halign = false;
   groupbox_add(groupbox, label);
   return label;
}

wo_t *settings_create_input(wo_t *groupbox, int y, char *text, void (*callback)(wo_t *wo, int window)) {
   wo_t *input = create_input(140, y, 140, 20);
   input_t *input_data = input->data;
   input_data->valign = true;
   //input_data->halign = true;
   input_data->return_func = callback;
   input_data->padding_left = 5;
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

void scroll(int deltaY, int offsetY, int window) {
   (void)offsetY;
   (void)window;

   scrollOffsetY = offsetY;

   clear();

   for(int i = 0; i < ui->wo_count; i++) {
      wo_t *wo = ui->wos[i];
      if(!wo || !wo->enabled) continue;
      ui->wos[i]->y -= deltaY;
   }

   ui_redraw(ui);

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
   label_t *colourbox = colour1_colourbox->data;
   colourbox->colour_bg = (uint16_t)hextouint(input->text + 2);
   ui_draw(ui);
}

void set_colour2(wo_t *wo, int window) {
   (void)window;
   input_t *input = wo->data;
   set_setting(SETTING_WIN_TITLEBARCOLOUR2, hextouint(input->text + 2));
   label_t *colourbox = colour2_colourbox->data;
   colourbox->colour_bg = (uint16_t)hextouint(input->text + 2);
   ui_draw(ui);
}

void set_window_bgcolour(wo_t *wo, int window) {
   (void)window;
   input_t *input = wo->data;
   set_setting(SETTING_WIN_BGCOLOUR, hextouint(input->text + 2));
   label_t *colourbox = windowbg_colourbox->data;
   colourbox->colour_bg = (uint16_t)hextouint(input->text + 2);
   ui_draw(ui);
}

void set_window_txtcolour(wo_t *wo, int window) {
   (void)window;
   input_t *input = wo->data;
   set_setting(SETTING_WIN_TXTCOLOUR, hextouint(input->text + 2));
   label_t *colourbox = windowtxt_colourbox->data;
   colourbox->colour_bg = (uint16_t)hextouint(input->text + 2);
   ui_draw(ui);
}

void set_font_padding(wo_t *wo, int window) {
   (void)window;
   input_t *input = wo->data;
   set_setting(SETTINGS_SYS_FONT_PADDING, strtoint(input->text));
   clear_w(window);
   ui_draw(ui);
}

void set_bgcolour(wo_t *wo, int window) {
   (void)window;
   input_t *input = wo->data;
   set_setting(SETTING_BGCOLOUR, hextouint(input->text + 2));
   label_t *colourbox = bgcolour_colourbox->data;
   colourbox->colour_bg = (uint16_t)hextouint(input->text + 2);
   ui_draw(ui);
}

void set_desktop_enabled(wo_t *wo, int window) {
   (void)window;
   input_t *input = wo->data;
   set_setting(SETTING_DESKTOP_ENABLED, strtoint(input->text));
}

void set_bgimage(wo_t *wo, int window) {
   (void)window;
   input_t *input = wo->data;
   if(!set_setting(SETTING_DESKTOP_BGIMG_PATH, (uint32_t)input->text)) {
      dialog_msg("Error", "Couldn't set background image");
      return;
   }
   uint16_t bgcolour = get_setting(SETTING_BGCOLOUR);
   char hexbuf[8];
   strcpy(hexbuf, "0x");
   uinttohexstr(bgcolour, hexbuf+2);
   set_input_text(bgcolour_input, hexbuf);
   label_t *colourbox = bgcolour_colourbox->data;
   colourbox->colour_bg = bgcolour;
   ui_draw(ui);
}

void set_font(wo_t *wo, int window) {
   (void)window;
   input_t *input = wo->data;
   if(!set_setting(SETTING_SYS_FONT_PATH, (uint32_t)input->text))
      dialog_msg("Error", "Couldn't set font");
   clear_w(window);
   ui_draw(ui);
   redraw_w(window);
}

void bgimg_browse_callback(char *path, int window) {
   set_input_text(bgimg_input, path);
   set_bgimage(bgimg_input, window);
}

void fontpath_browse_callback(char *path, int window) {
   set_input_text(fontpath_input, path);
   set_font(fontpath_input, window);
}

void bgimg_browse(wo_t *wo, int window) {
   (void)wo;
   (void)window;
   dialog_filepicker("/bmp", &bgimg_browse_callback);
}

void fontpath_browse(wo_t *wo, int window) {
   (void)wo;
   (void)window;
   dialog_filepicker("/font", &fontpath_browse_callback);
}

void set_gradientstyle(wo_t *wo, int index, int window) {
   (void)wo;
   (void)window;
   set_setting(SETTING_THEME_GRADIENTSTYLE, index);
}

void padding_increase(wo_t *wo, int window) {
   (void)wo;
   int padding = strtoint(((input_t*)fontpadding_input->data)->text);
   padding++;
   char buffer[4];
   inttostr(padding, buffer);
   set_input_text(fontpadding_input, buffer);
   set_font_padding(fontpadding_input, window);
   clear_w(window);
   ui_draw(ui);
}

void padding_decrease(wo_t *wo, int window) {
   (void)wo;
   int padding = strtoint(((input_t*)fontpadding_input->data)->text);
   padding--;
   char buffer[4];
   inttostr(padding, buffer);
   set_input_text(fontpadding_input, buffer);
   set_font_padding(fontpadding_input, window);
   clear_w(window);
   ui_draw(ui);
}

void desktop_enable_checkbox_callback(wo_t *wo, int window) {
   checkbox_t *check_data = wo->data;
   char buffer[4];
   inttostr(check_data->checked, buffer);
   set_input_text(desktopenabled_input, buffer);
   set_desktop_enabled(desktopenabled_input, window);
}

void _start() {
   set_window_title("System Settings");
   set_window_size(360, 280);
   
   override_draw(0);

   char buffer[256];

   int overall_y = 30;

   // register as dialog
   int index = get_free_dialog();
   if(index < 0) {
      printf("Couldn't create dialog\n");
      exit(0);
   }
   dialog_t *dialog = get_dialog(index);
   dialog_init(dialog, -1);
   ui = dialog->ui;

   // title
   wo_t *title_wo = create_wo(5, 10, 40, 20);
   title_wo->draw_func = &title_draw;
   ui_add(ui, title_wo);

   // theme settings
   wo_t *theme_group = create_groupbox(30, overall_y, 325, 100, "Theme settings");
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

   // theme gradient style
   label = settings_create_label(theme_group, y, "Gradient style");
   menu = create_menu(140, y, 140, 35);
   menu_data = menu->data;
   add_menu_item(menu, "Horizontal", &set_gradientstyle);
   add_menu_item(menu, "Vertical", &set_gradientstyle);
   menu_data->selected_index = get_setting(SETTING_THEME_GRADIENTSTYLE);
   groupbox_add(theme_group, menu);
   label->height = 35;
   y+=40;

   // theme colour 1
   settings_create_label(theme_group, y, "Colour 1");
   strcpy(buffer, "0x");
   uinttohexstr(get_setting(SETTING_WIN_TITLEBARCOLOUR), buffer+2);
   wo_t *input = settings_create_input(theme_group, y, buffer, &set_colour1);
   input->width-=20;
   colour1_colourbox = settings_create_colourbox(theme_group, y, get_setting(SETTING_WIN_TITLEBARCOLOUR));
   y+=25;

   // theme colour 2
   settings_create_label(theme_group, y, "Colour 2");
   strcpy(buffer, "0x");
   uinttohexstr(get_setting(SETTING_WIN_TITLEBARCOLOUR2), buffer+2);
   input = settings_create_input(theme_group, y, buffer, &set_colour2);
   colour2_colourbox = settings_create_colourbox(theme_group, y, get_setting(SETTING_WIN_TITLEBARCOLOUR2));
   input->width-=20;
   y+=25;

   // window background colour
   settings_create_label(theme_group, y, "Window background");
   strcpy(buffer, "0x");
   uinttohexstr(get_setting(SETTING_WIN_BGCOLOUR), buffer+2);
   input = settings_create_input(theme_group, y, buffer, &set_window_bgcolour);
   windowbg_colourbox = settings_create_colourbox(theme_group, y, get_setting(SETTING_WIN_BGCOLOUR));
   input->width-=20;
   y+=25;

   // window txt colour
   settings_create_label(theme_group, y, "Window text");
   strcpy(buffer, "0x");
   uinttohexstr(get_setting(SETTING_WIN_TXTCOLOUR), buffer+2);
   input = settings_create_input(theme_group, y, buffer, &set_window_txtcolour);
   windowtxt_colourbox = settings_create_colourbox(theme_group, y, get_setting(SETTING_WIN_TXTCOLOUR));
   input->width-=20;
   y+=25;

   groupbox_resize(theme_group, 285, y + 12);
   overall_y += y + 20;

   // font settings
   wo_t *font_group = create_groupbox(30, overall_y, 285, 100, "Font settings");
   ui_add(ui, font_group);
   y = 5;

   // font
   settings_create_label(font_group, y, "Font");
   strcpy(buffer, (char*)get_setting(SETTING_SYS_FONT_PATH));
   fontpath_input = settings_create_input(font_group, y, buffer, &set_font);
   fontpath_input->width-=20;
   wo_t *button = create_button(260, y, 20, 20, "o");
   set_button_release(button, &fontpath_browse);
   groupbox_add(font_group, button);
   y+=25;

   // font padding
   settings_create_label(font_group, y, "Padding");
   inttostr(get_setting(SETTINGS_SYS_FONT_PADDING), buffer);
   fontpadding_input = settings_create_input(font_group, y, buffer, &set_font_padding);
   fontpadding_input->width-=20;
   char btnbuf[2];
   btnbuf[0] = 0x80; // uparrow
   btnbuf[1] = '\0';
   button = create_button(260, y, 20, 10, btnbuf);
   set_button_release(button, &padding_increase);
   groupbox_add(font_group, button);
   btnbuf[0] = 0x81; // downarrow
   button = create_button(260, y+10, 20, 10, btnbuf);
   set_button_release(button, &padding_decrease);
   groupbox_add(font_group, button);
   y+=25;

   groupbox_resize(font_group, 285, y + 12);
   overall_y += y + 20;

   // desktop settings
   wo_t *desktop_group = create_groupbox(30, overall_y, 285, 90, "Desktop settings");
   ui_add(ui, desktop_group);
   y = 5;

   // desktop enabled
   settings_create_label(desktop_group, y, "Desktop enabled");
   inttostr(get_setting(SETTING_DESKTOP_ENABLED), buffer);
   desktopenabled_input = settings_create_input(desktop_group, y, buffer, &set_desktop_enabled);
   desktopenabled_input->width-=20;
   wo_t *checkbox = create_checkbox(260, y, get_setting(SETTING_DESKTOP_ENABLED));
   set_checkbox_release(checkbox, &desktop_enable_checkbox_callback);
   groupbox_add(desktop_group, checkbox);
   y+=25;

   // desktop bgimg
   settings_create_label(desktop_group, y, "Background image");
   strcpy(buffer, (char*)get_setting(SETTING_DESKTOP_BGIMG_PATH));
   bgimg_input = settings_create_input(desktop_group, y, buffer, &set_bgimage);
   bgimg_input->width-=20;
   button = create_button(260, y, 20, 20, "o");
   set_button_release(button, &bgimg_browse);
   groupbox_add(desktop_group, button);
   y+=25;

   // desktop bg colour
   settings_create_label(desktop_group, y, "Background colour");
   strcpy(buffer, "0x");
   uinttohexstr(get_setting(SETTING_BGCOLOUR), buffer+2);
   bgcolour_input = settings_create_input(desktop_group, y, buffer, &set_bgcolour);
   bgcolour_colourbox = settings_create_colourbox(desktop_group, y, get_setting(SETTING_BGCOLOUR));
   bgcolour_input->width-=20;
   y+=25;

   groupbox_resize(desktop_group, 285, y + 12);
   overall_y += y + 20;

   for(int i = 0; i < 8; i++)
      colourpicker_wos[i] = NULL;

   ui_draw(ui);

   create_scrollbar(&scroll, -1);
   dialog->content_height = overall_y;
   set_content_height(overall_y, -1);

   while(true) {
      yield();
   }
}