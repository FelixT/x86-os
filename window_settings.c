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

   char title[256] = "";
   sprintf(title, "Window settings for '%s'", settings->selected->title);
   window_writestrat(title, window->txtcolour, 10, 10, index);

   window_writestrat("Background Colour", window->txtcolour, 10, 38, index);
   window_writestrat("Text Colour", window->txtcolour, 10, 63, index);  
}

void window_settings_redraw(void *w) {
   gui_window_t *window = w;
   window_clearbuffer(window, window->bgcolour);
   window_settings_draw(w);
   window->needs_redraw = true;
   window_draw(window);
}

extern uint16_t gui_bg;

void window_settings_update(window_settings_t *settings) {
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

   window_settings_update(settings);
}

// callback from colourpicker
void window_settings_set_window_bgcolour_callback(uint16_t colour) {
   window_settings_t *settings = (window_settings_t*)getSelectedWindow()->state;
   gui_window_t *window = settings->selected;
   window->bgcolour = colour;
   window_clearbuffer(settings->selected, settings->selected->bgcolour);
   uinttohexstr(colour, settings->w_bgcolour_wo->text);

   window_settings_update(settings);
}

// callback from text windowobj
void window_settings_set_window_txtcolour(void *w) {
   window_settings_t *settings = (window_settings_t*)getSelectedWindow()->state;
   gui_window_t *window = settings->selected;
   uint16_t colour = (uint16_t)hextouint(((windowobj_t*)w)->text);
   window->txtcolour = colour;

   window_settings_update(settings);
}

// callback from colourpicker
void window_settings_set_window_txtcolour_callback(uint16_t colour) {
   window_settings_t *settings = (window_settings_t*)getSelectedWindow()->state;
   gui_window_t *window = settings->selected;
   window->txtcolour = colour;
   uinttohexstr(colour, settings->w_txtcolour_wo->text);

   window_settings_update(settings);
}

void window_settings_pickbgcolour(void *wo, int x, int y) {
   (void)wo;
   (void)x;
   (void)y;
   gui_window_t *parent = getSelectedWindow();
   int popup = windowmgr_add();
   window_settings_t *settings = (window_settings_t*)parent->state;
   uint16_t colour = (uint16_t)hextouint(settings->w_bgcolour_wo->text);
   window_popup_colourpicker(getWindow(popup), parent, &window_settings_set_window_bgcolour_callback, colour);
   window_draw_outline(getWindow(popup), false);
}

void window_settings_picktxtcolour(void *wo, int x, int y) {
   (void)wo;
   (void)x;
   (void)y;
   gui_window_t *parent = getSelectedWindow();
   int popup = windowmgr_add();
   window_settings_t *settings = (window_settings_t*)parent->state;
   uint16_t colour = (uint16_t)hextouint(settings->w_txtcolour_wo->text);
   window_popup_colourpicker(getWindow(popup), parent, &window_settings_set_window_txtcolour_callback, colour);
   window_draw_outline(getWindow(popup), false);
}

bool window_settings_init(gui_window_t *window, gui_window_t *selected) {
   if(selected == NULL)
      return false;

   if(selected->child_count == W_CHILDCOUNT)
      return false;

   window_settings_t *settings = malloc(sizeof(window_settings_t));
   window->state = (void*)settings;
   window->state_size = sizeof(window_settings_t);

   strcpy(window->title, "Window Settings");

   selected->children[selected->child_count++] = window; // treated as child window
   window->parent = selected;
   settings->window = window;
   settings->selected = selected;

   int x = 10 + font_width(25) + 10;
   int x2 = x + 104;
   // window settings

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

   window_resize(NULL, window, 340, 100, NULL);
   window_draw_outline(window, false);
   window_settings_redraw(window);

   window->draw_func = &window_settings_draw;

   return settings;
}