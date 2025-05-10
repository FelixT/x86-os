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

void window_settings_draw(void *w) {
   gui_window_t *window = (gui_window_t*)w;
   int index = get_window_index_from_pointer(window);
   window_settings_t *settings = window->state;
   if(!settings) return;
   if(settings->selected == (gui_window_t*)w)
      window_writestrat("System settings:", window->txtcolour, 10, 10, index);
   else if(settings->selected) {
      window_writestrat("Window settings:", window->txtcolour, 10, 10, index);
   }
   window_writestrat("Desktop Background Colour", window->txtcolour, 10, 40, index);
   window_writestrat("Desktop Background Image", window->txtcolour, 10, 70, index);
   window_writestrat("Window Background Colour", window->txtcolour, 10, 100, index);
   window_writestrat("Window Text Colour", window->txtcolour, 10, 130, index);  
   //window_writestrat("Text Padding", window->txtcolour, 10, 160, index);
   //window->needs_redraw = true;
}

void window_settings_redraw(void *w) {
   window_settings_draw(w);
   window_clearbuffer(w, ((gui_window_t*)w)->bgcolour);
   window_draw(w);
}

extern uint16_t gui_bg;

uint32_t hextouint(char *str) {
   uint32_t u = 0;

   while(*str != '\0') {
      if(*str >= '0' && *str <= '9')
         u = (u << 4) + (*str - '0');
      else if(*str >= 'A' && *str <= 'F')
         u = (u << 4) + (*str - 'A' + 10);
      else if(*str >= 'a' && *str <= 'f')
         u = (u << 4) + (*str - 'a' + 10);
      str++;
   }
   return u;
}

void window_settings_set_bgcolour(void *w) {
   gui_bg = (uint16_t)hextouint(((windowobj_t*)w)->text);
   gui_redrawall();
}

void window_settings_update(window_settings_t *settings) {
   if(settings->selected == settings->window)
      window_settings_draw(settings->selected);
   settings->selected->needs_redraw = true;
   window_draw(settings->selected);
}

void window_settings_set_window_bgcolour(void *w) {
   window_settings_t *settings = (window_settings_t*)getSelectedWindow()->state;
   gui_window_t *window = settings->selected;
   window->bgcolour = (uint16_t)hextouint(((windowobj_t*)w)->text);
   window_clearbuffer(settings->selected, settings->selected->bgcolour);
   window_settings_update(settings);
}

void window_settings_set_window_txtcolour(void *w) {
   window_settings_t *settings = (window_settings_t*)getSelectedWindow()->state;
   gui_window_t *window = settings->selected;
   window->txtcolour = (uint16_t)hextouint(((windowobj_t*)w)->text);
   window_settings_update(settings);
}

window_settings_t *window_settings_init(gui_window_t *window, gui_window_t *selected) {
   window_settings_t *settings = malloc(sizeof(window_settings_t));

   strcpy(window->title, "Settings");
   window->draw_func = &window_settings_draw;
   window_resize(NULL, window, 400, 300);

   if(selected == NULL) {
      selected = window;
   }
   settings->window = window;
   settings->selected = selected;

   windowobj_t *d_bgcolour_wo = (windowobj_t*)malloc(sizeof(windowobj_t));
   windowobj_t *d_bgimg_wo = (windowobj_t*)malloc(sizeof(windowobj_t));
   windowobj_t *w_bgcolour_wo = (windowobj_t*)malloc(sizeof(windowobj_t));
   windowobj_t *w_txtcolour_wo = (windowobj_t*)malloc(sizeof(windowobj_t));
   settings->d_bgcolour_wo = d_bgcolour_wo;
   settings->d_bgimg_wo = d_bgimg_wo;
   settings->w_bgcolour_wo = w_bgcolour_wo;
   settings->w_txtcolour_wo = w_txtcolour_wo;

   int y = 35;
   windowobj_init(d_bgcolour_wo, &window->surface);
   d_bgcolour_wo->type = WO_TEXT;
   d_bgcolour_wo->x = 250;
   d_bgcolour_wo->y = y;
   d_bgcolour_wo->width = 100;
   d_bgcolour_wo->height = 20;
   d_bgcolour_wo->text = malloc(40);
   d_bgcolour_wo->return_func = &window_settings_set_bgcolour;
   uinttohexstr(gui_bg, d_bgcolour_wo->text);
   window->window_objects[window->window_object_count++] = d_bgcolour_wo;

   y += 30;
   windowobj_init(d_bgimg_wo, &window->surface);
   d_bgimg_wo->type = WO_TEXT;
   d_bgimg_wo->x = 250;
   d_bgimg_wo->y = y;
   d_bgimg_wo->width = 100;
   d_bgimg_wo->height = 20;
   d_bgimg_wo->text = malloc(100);
   strcpy(d_bgimg_wo->text, "/bg/bg16.bmp");
   //d_bgcolour_wo->return_func = &window_settings_set_bgcolour;
   window->window_objects[window->window_object_count++] = d_bgimg_wo;

   y += 30;
   windowobj_init(w_bgcolour_wo, &window->surface);
   w_bgcolour_wo->type = WO_TEXT;
   w_bgcolour_wo->x = 250;
   w_bgcolour_wo->y = y;
   w_bgcolour_wo->width = 100;
   w_bgcolour_wo->height = 20;
   w_bgcolour_wo->text = malloc(100);
   w_bgcolour_wo->return_func = &window_settings_set_window_bgcolour;
   uinttohexstr(selected->bgcolour, w_bgcolour_wo->text);
   window->window_objects[window->window_object_count++] = w_bgcolour_wo;

   y += 30;
   windowobj_init(w_txtcolour_wo, &window->surface);
   w_txtcolour_wo->type = WO_TEXT;
   w_txtcolour_wo->x = 250;
   w_txtcolour_wo->y = y;
   w_txtcolour_wo->width = 100;
   w_txtcolour_wo->height = 20;
   w_txtcolour_wo->text = malloc(100);
   w_txtcolour_wo->return_func = &window_settings_set_window_txtcolour;
   uinttohexstr(selected->txtcolour, w_txtcolour_wo->text);
   window->window_objects[window->window_object_count++] = w_txtcolour_wo;

   windowobj_redraw(window, d_bgcolour_wo);
   windowobj_redraw(window, d_bgimg_wo);
   windowobj_redraw(window, w_bgcolour_wo);
   windowobj_redraw(window, w_txtcolour_wo);

   window_settings_redraw(window);

   return settings;
}