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
   if(settings->selected == (gui_window_t*)w) {
      strcpy(window->title, "System Settings");
      window_writestrat("System settings:", window->txtcolour, 10, 10, index);
   } else if(settings->selected) {
      strcpy(window->title, "Window Settings");
      char title[256];
      sprintf(title, "Window settings for %s", settings->selected->title);
      window_writestrat(title, window->txtcolour, 10, 10, index);
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

extern uint16_t default_window_bgcolour;
extern uint16_t default_window_txtcolour;

// callback from text windowobj
void window_settings_set_window_bgcolour(void *w) {
   window_settings_t *settings = (window_settings_t*)getSelectedWindow()->state;
   gui_window_t *window = settings->selected;
   uint16_t colour = (uint16_t)hextouint(((windowobj_t*)w)->text);
   window->bgcolour = colour;
   window_clearbuffer(settings->selected, settings->selected->bgcolour);

   if(settings->selected == settings->window) {
      // update system settings
      default_window_bgcolour = colour;
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
      default_window_bgcolour = colour;
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
      default_window_txtcolour = colour;
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
      default_window_txtcolour = colour;
   }
   window_settings_update(settings);
}

void window_settings_set_bgimg(void *w) {
   fat_dir_t *entry = fat_parse_path(((windowobj_t*)w)->text, true);
   if(entry == NULL) {
      debug_writestr("Image not found\n");
      return;
   }

   uint8_t *gui_bgimage = fat_read_file(entry->firstClusterNo, entry->fileSize);
   desktop_setbgimg(gui_bgimage);

   gui_redrawall();
}

void window_settings_bgimg_callback(char *path) {
   windowobj_t *wo = ((window_settings_t*)getSelectedWindow()->state)->d_bgimg_wo;
   strcpy(wo->text, path);
   wo->textpos = strlen(wo->text);
   window_settings_set_bgimg(wo);
}

void window_settings_browesebg(void *w, void *regs) {
   (void)w;
   (void)regs;
   gui_window_t *parent = getSelectedWindow();
   int popup = windowmgr_add();
   window_popup_filepicker(getWindow(popup), parent, &window_settings_bgimg_callback);
}

void window_settings_pickdbgcolour(void *w, void *regs) {
   (void)w;
   (void)regs;
   gui_window_t *parent = getSelectedWindow();
   int popup = windowmgr_add();
   window_popup_colourpicker(getWindow(popup), parent, &window_settings_set_bgcolour_callback);
}

void window_settings_pickbgcolour(void *w, void *regs) {
   (void)regs;
   (void)w;
   gui_window_t *parent = getSelectedWindow();
   int popup = windowmgr_add();
   window_popup_colourpicker(getWindow(popup), parent, &window_settings_set_window_bgcolour_callback);
}

void window_settings_picktxtcolour(void *w, void *regs) {
   (void)regs;
   (void)w;
   gui_window_t *parent = getSelectedWindow();
   int popup = windowmgr_add();
   window_popup_colourpicker(getWindow(popup), parent, &window_settings_set_window_txtcolour_callback);
}

window_settings_t *window_settings_init(gui_window_t *window, gui_window_t *selected) {
   window_settings_t *settings = malloc(sizeof(window_settings_t));

   strcpy(window->title, "Settings");
   window->draw_func = &window_settings_draw;
   window_resize(NULL, window, 415, 180);

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
   d_bgcolour_wo->x = 220;
   d_bgcolour_wo->y = y;
   d_bgcolour_wo->width = 130;
   d_bgcolour_wo->height = 20;
   d_bgcolour_wo->text = malloc(40);
   d_bgcolour_wo->return_func = &window_settings_set_bgcolour;
   uinttohexstr(gui_bg, d_bgcolour_wo->text);
   d_bgcolour_wo->textpos = strlen(d_bgcolour_wo->text);
   window->window_objects[window->window_object_count++] = d_bgcolour_wo;

   // pick button
   windowobj_t *d_bgpickbtn = (windowobj_t*)malloc(sizeof(windowobj_t));
   windowobj_init(d_bgpickbtn, &window->surface);
   d_bgpickbtn->type = WO_BUTTON;
   d_bgpickbtn->x = 355;
   d_bgpickbtn->y = y;
   d_bgpickbtn->width = 50;
   d_bgpickbtn->height = 20;
   d_bgpickbtn->text = malloc(strlen("Pick")+1);
   strcpy(d_bgpickbtn->text, "Pick");
   d_bgpickbtn->click_func = &window_settings_pickdbgcolour;
   window->window_objects[window->window_object_count++] = d_bgpickbtn;

   y += 30;
   windowobj_init(d_bgimg_wo, &window->surface);
   d_bgimg_wo->type = WO_TEXT;
   d_bgimg_wo->x = 220;
   d_bgimg_wo->y = y;
   d_bgimg_wo->width = 130;
   d_bgimg_wo->height = 20;
   d_bgimg_wo->text = malloc(100);
   strcpy(d_bgimg_wo->text, "/bmp/bg16.bmp");
   d_bgimg_wo->textpos = strlen(d_bgimg_wo->text);
   d_bgimg_wo->return_func = &window_settings_set_bgimg;
   //d_bgcolour_wo->return_func = &window_settings_set_bgcolour;
   window->window_objects[window->window_object_count++] = d_bgimg_wo;

   // browse button
   windowobj_t *browsebtn = (windowobj_t*)malloc(sizeof(windowobj_t));
   windowobj_init(browsebtn, &window->surface);
   browsebtn->type = WO_BUTTON;
   browsebtn->x = 355;
   browsebtn->y = y;
   browsebtn->width = 50;
   browsebtn->height = 20;
   browsebtn->text = malloc(strlen("Browse")+1);
   strcpy(browsebtn->text, "Browse");
   browsebtn->click_func = &window_settings_browesebg;
   window->window_objects[window->window_object_count++] = browsebtn;

   y += 30;
   windowobj_init(w_bgcolour_wo, &window->surface);
   w_bgcolour_wo->type = WO_TEXT;
   w_bgcolour_wo->x = 220;
   w_bgcolour_wo->y = y;
   w_bgcolour_wo->width = 130;
   w_bgcolour_wo->height = 20;
   w_bgcolour_wo->text = malloc(100);
   w_bgcolour_wo->return_func = &window_settings_set_window_bgcolour;
   uinttohexstr(selected->bgcolour, w_bgcolour_wo->text);
   w_bgcolour_wo->textpos = strlen(w_bgcolour_wo->text);
   window->window_objects[window->window_object_count++] = w_bgcolour_wo;

   // pick button
   windowobj_t *bgpickbtn = (windowobj_t*)malloc(sizeof(windowobj_t));
   windowobj_init(bgpickbtn, &window->surface);
   bgpickbtn->type = WO_BUTTON;
   bgpickbtn->x = 355;
   bgpickbtn->y = y;
   bgpickbtn->width = 50;
   bgpickbtn->height = 20;
   bgpickbtn->text = malloc(strlen("Pick")+1);
   strcpy(bgpickbtn->text, "Pick");
   bgpickbtn->click_func = &window_settings_pickbgcolour;
   window->window_objects[window->window_object_count++] = bgpickbtn;

   y += 30;
   windowobj_init(w_txtcolour_wo, &window->surface);
   w_txtcolour_wo->type = WO_TEXT;
   w_txtcolour_wo->x = 220;
   w_txtcolour_wo->y = y;
   w_txtcolour_wo->width = 130;
   w_txtcolour_wo->height = 20;
   w_txtcolour_wo->text = malloc(100);
   w_txtcolour_wo->return_func = &window_settings_set_window_txtcolour;
   uinttohexstr(selected->txtcolour, w_txtcolour_wo->text);
   w_txtcolour_wo->textpos = strlen(w_txtcolour_wo->text);
   window->window_objects[window->window_object_count++] = w_txtcolour_wo;

   // pick button
   windowobj_t *txtpickbtn = (windowobj_t*)malloc(sizeof(windowobj_t));
   windowobj_init(txtpickbtn, &window->surface);
   txtpickbtn->type = WO_BUTTON;
   txtpickbtn->x = 355;
   txtpickbtn->y = y;
   txtpickbtn->width = 50;
   txtpickbtn->height = 20;
   txtpickbtn->text = malloc(strlen("Pick")+1);
   strcpy(txtpickbtn->text, "Pick");
   txtpickbtn->click_func = &window_settings_picktxtcolour;
   window->window_objects[window->window_object_count++] = txtpickbtn;

   windowobj_redraw(window, d_bgcolour_wo);
   windowobj_redraw(window, d_bgimg_wo);
   windowobj_redraw(window, w_bgcolour_wo);
   windowobj_redraw(window, w_txtcolour_wo);
   windowobj_redraw(window, browsebtn);

   window_settings_redraw(window);

   return settings;
}