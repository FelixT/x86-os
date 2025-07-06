// another mini program
// pretty similar to how a usermode program would be structured
// and ideally this would be one

#include "window_settings.h"
#include "window.h"
#include "windowobj.h"
#include "lib/string.h"
#include "memory.h"
#include "windowmgr.h"
#include "gui.h"

#include "window_popup.h"

void window_settings_draw(void *w) {
   gui_window_t *window = (gui_window_t*)w;
   int index = get_window_index_from_pointer(window);
   window_settings_t *settings = window->state;
   if(!settings) return;
   draw_rect(&window->surface, window->bgcolour, 0, 0, 200, 330);

   if(settings->selected == (gui_window_t*)w) {
      strcpy(window->title, "System Settings");
      window_writestrat("System settings:", window->txtcolour, 10, 10, index);

      window_writestrat("Desktop Background Colour", window->txtcolour, 10, 38, index);
      window_writestrat("Desktop Background Image", window->txtcolour, 10, 68, index);
      window_writestrat("Window Background Colour", window->txtcolour, 10, 98, index);
      window_writestrat("Window Text Colour", window->txtcolour, 10, 128, index);  
      window_writestrat("Text Padding", window->txtcolour, 10, 158, index);  
      window_writestrat("Theme", window->txtcolour, 10, 188, index);  
      window_writestrat("Gradient Type", window->txtcolour, 10, 228, index);  
      window_writestrat("Theme Colour", window->txtcolour, 10, 268, index);  
      window_writestrat("Theme Colour 2", window->txtcolour, 10, 298, index);  

   } else if(settings->selected) {
      strcpy(window->title, "Window Settings");
      char title[256];
      sprintf(title, "Window settings for '%s'", settings->selected->title);
      window_writestrat(title, window->txtcolour, 10, 10, index);

      window_writestrat("Background Colour", window->txtcolour, 10, 38, index);
      window_writestrat("Text Colour", window->txtcolour, 10, 68, index);  
   }
   draw_line(&window->surface, rgb16(200, 200, 200), 10, 20, false, font_width(strlen(window->title)));
   //window->needs_redraw = true;
}

void window_settings_redraw(void *w) {
   window_settings_draw(w);
   window_clearbuffer(w, ((gui_window_t*)w)->bgcolour);
   window_draw(w);
}

extern uint16_t gui_bg;

void window_settings_update(window_settings_t *settings) {
   if(settings->selected == settings->window)
      window_settings_draw(settings->selected);
   settings->selected->needs_redraw = true;
   window_draw(settings->selected);
}

// callback from text windowobj
void window_settings_set_bgcolour(void *w) {
   gui_bg = (uint16_t)hextouint(((windowobj_t*)w)->text);
   gui_redrawall();
}

// callback from colourpicker
void window_settings_set_bgcolour_callback(uint16_t colour) {
   window_settings_t *settings = (window_settings_t*)getSelectedWindow()->state;
   gui_bg = colour;
   uinttohexstr(colour, settings->d_bgcolour_wo->text);
   gui_redrawall();
}

// callback from text windowobj
void window_settings_set_window_bgcolour(void *w) {
   window_settings_t *settings = (window_settings_t*)getSelectedWindow()->state;
   gui_window_t *window = settings->selected;
   uint16_t colour = (uint16_t)hextouint(((windowobj_t*)w)->text);
   window->bgcolour = colour;
   window_clearbuffer(settings->selected, settings->selected->bgcolour);

   if(settings->selected == settings->window) {
      // update system settings
      windowmgr_get_settings()->default_window_bgcolour = colour;
   }
   window_settings_update(settings);
}

// callback from colourpicker
void window_settings_set_window_bgcolour_callback(uint16_t colour) {
   window_settings_t *settings = (window_settings_t*)getSelectedWindow()->state;
   gui_window_t *window = settings->selected;
   window->bgcolour = colour;
   window_clearbuffer(settings->selected, settings->selected->bgcolour);
   uinttohexstr(colour, settings->w_bgcolour_wo->text);

   if(settings->selected == settings->window) {
      // update system settings
      windowmgr_get_settings()->default_window_bgcolour = colour;
   }
   window_settings_update(settings);
}

// callback from text windowobj
void window_settings_set_window_txtcolour(void *w) {
   window_settings_t *settings = (window_settings_t*)getSelectedWindow()->state;
   gui_window_t *window = settings->selected;
   uint16_t colour = (uint16_t)hextouint(((windowobj_t*)w)->text);
   window->txtcolour = colour;

   if(settings->selected == settings->window) {
      // update system settings
      windowmgr_get_settings()->default_window_txtcolour = colour;
   }
   window_settings_update(settings);
}

// callback from colourpicker
void window_settings_set_window_txtcolour_callback(uint16_t colour) {
   window_settings_t *settings = (window_settings_t*)getSelectedWindow()->state;
   gui_window_t *window = settings->selected;
   window->txtcolour = colour;
   uinttohexstr(colour, settings->w_txtcolour_wo->text);

   if(settings->selected == settings->window) {
      // update system settings
      windowmgr_get_settings()->default_window_txtcolour = colour;
   }
   window_settings_update(settings);
}

void window_settings_set_bgimg(void *w) {
   fat_dir_t *entry = fat_parse_path(((windowobj_t*)w)->text, true);
   if(entry == NULL) {
      debug_writestr("Image not found\n");
      return;
   }

   strcpy(windowmgr_get_settings()->desktop_bgimg, ((windowobj_t*)w)->text);
   uint8_t *gui_bgimage = fat_read_file(entry->firstClusterNo, entry->fileSize);
   desktop_setbgimg(gui_bgimage);

   gui_redrawall();
}

void window_settings_bgimg_callback(char *path) {
   windowobj_t *wo = ((window_settings_t*)getSelectedWindow()->state)->d_bgimg_wo;
   strcpy(wo->text, path);
   wo->textpos = strlen(wo->text);
   wo->cursor_textpos =  wo->textpos;
   window_settings_set_bgimg(wo);
}

void window_settings_browesebg(void *w, void *regs) {
   (void)w;
   (void)regs;
   gui_window_t *parent = getSelectedWindow();
   int popup = windowmgr_add();
   window_popup_filepicker(getWindow(popup), parent, &window_settings_bgimg_callback);
   window_draw_outline(getWindow(popup), false);
}

void window_settings_pickdbgcolour(void *w, void *regs) {
   (void)w;
   (void)regs;
   gui_window_t *parent = getSelectedWindow();
   int popup = windowmgr_add();
   window_popup_colourpicker(getWindow(popup), parent, &window_settings_set_bgcolour_callback);
   window_draw_outline(getWindow(popup), false);
}

void window_settings_pickbgcolour(void *w, void *regs) {
   (void)regs;
   (void)w;
   gui_window_t *parent = getSelectedWindow();
   int popup = windowmgr_add();
   window_popup_colourpicker(getWindow(popup), parent, &window_settings_set_window_bgcolour_callback);
   window_draw_outline(getWindow(popup), false);
}

void window_settings_picktxtcolour(void *w, void *regs) {
   (void)regs;
   (void)w;
   gui_window_t *parent = getSelectedWindow();
   int popup = windowmgr_add();
   window_popup_colourpicker(getWindow(popup), parent, &window_settings_set_window_txtcolour_callback);
   window_draw_outline(getWindow(popup), false);
}

void window_settings_set_txtpadding(void *w) {
   uint16_t padding = (uint16_t)hextouint(((windowobj_t*)w)->text);
   getFont()->padding = padding;
}

void window_settings_set_theme_classic() {
   windowmgr_get_settings()->theme = 0; // classic
   windowmgr_get_settings()->titlebar_colour = COLOUR_TITLEBAR_CLASSIC;
   window_settings_t *settings = (window_settings_t*)getSelectedWindow()->state;
   uinttohexstr(COLOUR_TITLEBAR_CLASSIC, settings->theme_colour_wo->text);
   gui_redrawall();
}

void window_settings_set_theme_gradient() {
   windowmgr_get_settings()->theme = 1; // gradient
   windowmgr_get_settings()->titlebar_colour = COLOUR_TITLEBAR_COLOUR1;
   windowmgr_get_settings()->titlebar_colour2 = COLOUR_TITLEBAR_COLOUR2;
   window_settings_t *settings = (window_settings_t*)getSelectedWindow()->state;
   uinttohexstr(COLOUR_TITLEBAR_COLOUR1, settings->theme_colour_wo->text);
   uinttohexstr(COLOUR_TITLEBAR_COLOUR2, settings->theme_colour2_wo->text);
   gui_redrawall();
}

void window_settings_set_theme_gradient_horizontal() {
   windowmgr_get_settings()->titlebar_gradientstyle = 0; // horizontal
   gui_redrawall();
}

void window_settings_set_theme_gradient_vertical() {
   windowmgr_get_settings()->titlebar_gradientstyle = 1; // vertical
   gui_redrawall();
}

void window_settings_set_theme_colour(void *w) {
   window_settings_t *settings = (window_settings_t*)getSelectedWindow()->state;
   uint16_t colour = (uint16_t)hextouint(((windowobj_t*)w)->text);
   windowmgr_get_settings()->titlebar_colour = colour;
   uinttohexstr(colour, settings->theme_colour_wo->text);
   gui_redrawall();
}

void window_settings_set_theme_colour_callback(uint16_t colour) {
   window_settings_t *settings = (window_settings_t*)getSelectedWindow()->state;
   windowmgr_get_settings()->titlebar_colour = colour;
   uinttohexstr(colour, settings->theme_colour_wo->text);
   gui_redrawall();
}

void window_settings_set_theme_colour2(void *w) {
   window_settings_t *settings = (window_settings_t*)getSelectedWindow()->state;
   uint16_t colour = (uint16_t)hextouint(((windowobj_t*)w)->text);
   windowmgr_get_settings()->titlebar_colour2 = colour;
   uinttohexstr(colour, settings->theme_colour2_wo->text);
   gui_redrawall();
}

void window_settings_set_theme_colour2_callback(uint16_t colour) {
   window_settings_t *settings = (window_settings_t*)getSelectedWindow()->state;
   windowmgr_get_settings()->titlebar_colour2 = colour;
   uinttohexstr(colour, settings->theme_colour2_wo->text);
   gui_redrawall();
}

void window_settings_pick_theme_colour(void *w, void *regs) {
   (void)regs;
   (void)w;
   gui_window_t *parent = getSelectedWindow();
   int popup = windowmgr_add();
   window_popup_colourpicker(getWindow(popup), parent, &window_settings_set_theme_colour_callback);
   window_draw_outline(getWindow(popup), false);
}

void window_settings_pick_theme_colour2(void *w, void *regs) {
   (void)regs;
   (void)w;
   gui_window_t *parent = getSelectedWindow();
   int popup = windowmgr_add();
   window_popup_colourpicker(getWindow(popup), parent, &window_settings_set_theme_colour2_callback);
   window_draw_outline(getWindow(popup), false);
}

window_settings_t *window_settings_init(gui_window_t *window, gui_window_t *selected) {
   window_settings_t *settings = malloc(sizeof(window_settings_t));

   strcpy(window->title, "Settings");
   window->draw_func = &window_settings_draw;

   if(selected == NULL) {
      selected = window;
   } else {
      selected->children[selected->child_count++] = window;
   }
   settings->window = window;
   settings->selected = selected;

   // common
   windowobj_t *w_bgcolour_wo = (windowobj_t*)malloc(sizeof(windowobj_t));
   windowobj_t *w_txtcolour_wo = (windowobj_t*)malloc(sizeof(windowobj_t));
   settings->w_bgcolour_wo = w_bgcolour_wo;
   settings->w_txtcolour_wo = w_txtcolour_wo;

   if(settings->selected == settings->window) {
      // system settings
      window_resize(NULL, window, 370, 340);

      int y = 35;

      char text[256];

      // desktop background colour
      uinttohexstr(gui_bg, text);
      settings->d_bgcolour_wo = window_create_text(window, 200, y, text);
      settings->d_bgcolour_wo->return_func = &window_settings_set_bgcolour;
      window_create_button(window, 304, y, "Pick", &window_settings_pickdbgcolour);

      // desktop background image
      y += 30;
      settings->d_bgimg_wo = window_create_text(window, 200, y, windowmgr_get_settings()->desktop_bgimg);
      settings->d_bgimg_wo->return_func = &window_settings_set_bgimg;
      window_create_button(window, 304, y, "Browse", &window_settings_browesebg);

      // window background colour
      y += 30;
      uinttohexstr(selected->bgcolour, text);
      settings->w_bgcolour_wo = window_create_text(window, 200, y, text);
      settings->w_bgcolour_wo->return_func = &window_settings_set_window_bgcolour;
      window_create_button(window, 304, y, "Pick", &window_settings_pickbgcolour);

      // window text colour
      y += 30;
      uinttohexstr(selected->txtcolour, text);
      settings->w_txtcolour_wo = window_create_text(window, 200, y, text);
      settings->w_txtcolour_wo->return_func = &window_settings_set_window_txtcolour;
      window_create_button(window, 304, y, "Pick", &window_settings_picktxtcolour);

      // text padding
      y += 30;
      uinttostr(getFont()->padding, text);
      settings->theme_txtpadding_wo = window_create_text(window, 200, y, text);
      settings->theme_txtpadding_wo->return_func = &window_settings_set_txtpadding;

      // theme menu
      y += 30;
      windowobj_menu_t *menuitems = (windowobj_menu_t*)malloc(sizeof(windowobj_menu_t) * 2);
      strcpy(menuitems[0].text, "Classic");
      menuitems[0].func = &window_settings_set_theme_classic;
      menuitems[0].disabled = false;
      strcpy(menuitems[1].text, "Gradient");
      menuitems[1].func = &window_settings_set_theme_gradient;
      menuitems[1].disabled = false;
      settings->theme_wo = window_create_menu(window, 200, y, menuitems, 2);
      settings->theme_wo->menuselected = windowmgr_get_settings()->theme; // classic

      // gradient style
      y += 40;
      menuitems = (windowobj_menu_t*)malloc(sizeof(windowobj_menu_t) * 2);
      strcpy(menuitems[0].text, "Horizontal");
      menuitems[0].func = &window_settings_set_theme_gradient_horizontal;
      menuitems[0].disabled = false;
      strcpy(menuitems[1].text, "Vertical");
      menuitems[1].func = &window_settings_set_theme_gradient_vertical;
      menuitems[1].disabled = false;
      settings->theme_gradientstyle_wo = window_create_menu(window, 200, y, menuitems, 2);
      settings->theme_gradientstyle_wo->menuselected = windowmgr_get_settings()->titlebar_gradientstyle; // horizontal

      // theme colour 1
      y += 40;
      uinttohexstr(windowmgr_get_settings()->titlebar_colour, text);
      settings->theme_colour_wo = window_create_text(window, 200, y, text);
      settings->theme_colour_wo->return_func = &window_settings_set_theme_colour;
      window_create_button(window, 304, y, "Pick", &window_settings_pick_theme_colour);

      // theme colour 2
      y += 30;
      uinttohexstr(windowmgr_get_settings()->titlebar_colour2, text);
      settings->theme_colour2_wo = window_create_text(window, 200, y, text);
      settings->theme_colour2_wo->return_func = &window_settings_set_theme_colour2;
      window_create_button(window, 304, y, "Pick", &window_settings_pick_theme_colour2);

   } else {
      // window settings
         window_resize(NULL, window, 370, 180);

      settings->d_bgcolour_wo = NULL;
      settings->d_bgimg_wo = NULL;
      settings->theme_wo = NULL;
      settings->theme_gradientstyle_wo = NULL;
      settings->theme_colour_wo = NULL;
      settings->theme_colour2_wo = NULL;
      settings->theme_txtpadding_wo = NULL;

      int y = 35;
      char text[256];

      // window background colour
      uinttohexstr(selected->bgcolour, text);
      settings->w_bgcolour_wo = window_create_text(window, 200, y, text);
      settings->w_bgcolour_wo->return_func = &window_settings_set_window_bgcolour;
      window_create_button(window, 304, y, "Pick", &window_settings_pickbgcolour);

      // window text colour
      y += 30;
      uinttohexstr(selected->txtcolour, text);
      settings->w_txtcolour_wo = window_create_text(window, 200, y, text);
      settings->w_txtcolour_wo->return_func = &window_settings_set_window_txtcolour;
      window_create_button(window, 304, y, "Pick", &window_settings_picktxtcolour);

   }

   window_settings_redraw(window);

   return settings;
}