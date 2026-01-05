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

   int x = 10 + font_width(25) + 10;
   draw_rect(&window->surface, window->bgcolour, 0, 0, x, window->height - TITLEBAR_HEIGHT);

   if(settings->selected == (gui_window_t*)w) {
      // removed
   } else if(settings->selected) {
      strcpy(window->title, "Window Settings");
      char title[256];
      sprintf(title, "Window settings for '%s'", settings->selected->title);
      window_writestrat(title, window->txtcolour, 10, 10, index);

      window_writestrat("Background Colour", window->txtcolour, 10, 38, index);
      window_writestrat("Text Colour", window->txtcolour, 10, 63, index);  
   }
   //window->needs_redraw = true;
}

void window_settings_redraw(void *w) {
   window_clearbuffer(w, ((gui_window_t*)w)->bgcolour);
   window_settings_draw(w);
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

void window_settings_pickbgcolour(void *w, void *regs) {
   (void)regs;
   (void)w;
   gui_window_t *parent = getSelectedWindow();
   int popup = windowmgr_add();
   window_settings_t *settings = (window_settings_t*)parent->state;
   uint16_t colour = (uint16_t)hextouint(settings->w_bgcolour_wo->text);
   window_popup_colourpicker(getWindow(popup), parent, &window_settings_set_window_bgcolour_callback, colour);
   window_draw_outline(getWindow(popup), false);
}

void window_settings_picktxtcolour(void *w, void *regs) {
   (void)regs;
   (void)w;
   gui_window_t *parent = getSelectedWindow();
   int popup = windowmgr_add();
   window_settings_t *settings = (window_settings_t*)parent->state;
   uint16_t colour = (uint16_t)hextouint(settings->w_txtcolour_wo->text);
   window_popup_colourpicker(getWindow(popup), parent, &window_settings_set_window_txtcolour_callback, colour);
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

   int x = 10 + font_width(25) + 10;
   int x2 = x + 104;
   if(settings->selected == settings->window) {
      // system settings
      // removed

   } else {
      // window settings
      window_resize(NULL, window, 370, 180);

      settings->d_bgcolour_wo = NULL;
      settings->d_bgcolourpick_wo = NULL;
      settings->d_bgimg_wo = NULL;
      settings->d_bgimgpick_wo = NULL;
      settings->theme_wo = NULL;
      settings->theme_gradientstyle_wo = NULL;
      settings->theme_colour_wo = NULL;
      settings->theme_colourpick_wo = NULL;
      settings->theme_colour2_wo = NULL;
      settings->theme_colour2pick_wo = NULL;
      settings->theme_txtpadding_wo = NULL;
      settings->theme_fontpath_wo = NULL;

      int y = 35;
      char text[256];

      // window background colour
      uinttohexstr(selected->bgcolour, text);
      settings->w_bgcolour_wo = window_create_text(window, x, y, text);
      settings->w_bgcolour_wo->oneline = true;
      settings->w_bgcolour_wo->return_func = &window_settings_set_window_bgcolour;
      window_create_button(window, x2, y, "Pick", &window_settings_pickbgcolour);

      // window text colour
      y += 25;
      uinttohexstr(selected->txtcolour, text);
      settings->w_txtcolour_wo = window_create_text(window, x, y, text);
      settings->w_txtcolour_wo->oneline = true;
      settings->w_txtcolour_wo->return_func = &window_settings_set_window_txtcolour;
      window_create_button(window, x2, y, "Pick", &window_settings_picktxtcolour);

   }

   window_settings_redraw(window);

   return settings;
}