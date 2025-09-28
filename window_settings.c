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
   int x2 = x + 104;
   draw_rect(&window->surface, window->bgcolour, 0, 0, x, window->height - TITLEBAR_HEIGHT);

   int y = -window->scrolledY + 10;

   if(settings->selected == (gui_window_t*)w) {
      strcpy(window->title, "System Settings");
      window_writestrat("System settings:", window->txtcolour, 10, y, index);
      y += 10;
      draw_line(&window->surface, rgb16(200, 200, 200), 10, y, false, font_width(strlen(window->title)));
      y += 18;
      window_writestrat("Desktop Background Colour", window->txtcolour, 10, y, index);
      y += 25;
      window_writestrat("Desktop Background Image", window->txtcolour, 10, y, index);
      y += 25;
      window_writestrat("Window Background Colour", window->txtcolour, 10, y, index);
      y += 25;
      window_writestrat("Window Text Colour", window->txtcolour, 10, y, index);
      y += 25;
      window_writestrat("Text Padding", window->txtcolour, 10, y, index);
      y += 25;
      window_writestrat("Theme", window->txtcolour, 10, y, index);
      y += 35;
      window_writestrat("Gradient Type", window->txtcolour, 10, y, index);
      y += 35;
      window_writestrat("Theme Colour", window->txtcolour, 10, y, index);
      y += 25;
      window_writestrat("Theme Colour 2", window->txtcolour, 10, y, index);
      y += 25;
      window_writestrat("Font", window->txtcolour, 10, y, index);

      // update positions
      // todo: put these as children on a canvas
      settings->d_bgcolour_wo->x = x;
      settings->d_bgcolourpick_wo->x = x2;
      settings->d_bgimg_wo->x = x;
      settings->d_bgimgpick_wo->x = x2;
      settings->w_bgcolour_wo->x = x;
      settings->w_bgcolourpick_wo->x = x2;
      settings->w_txtcolour_wo->x = x;
      settings->w_txtcolourpick_wo->x = x2;
      settings->theme_txtpadding_wo->x = x;
      settings->theme_wo->x = x;
      settings->theme_gradientstyle_wo->x = x;
      settings->theme_colour_wo->x = x;
      settings->theme_colourpick_wo->x = x2;
      settings->theme_colour2_wo->x = x;
      settings->theme_colour2pick_wo->x = x2;
      settings->theme_fontpath_wo->x = x;
      settings->theme_fontpathpick_wo->x = x2;

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
   window_settings_t *settings = (window_settings_t*)parent->state;
   uint16_t colour = (uint16_t)hextouint(settings->d_bgcolour_wo->text);
   window_popup_colourpicker(getWindow(popup), parent, &window_settings_set_bgcolour_callback, colour);
   window_draw_outline(getWindow(popup), false);
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

void window_settings_set_txtpadding(void *w) {
   uint16_t padding = (uint16_t)hextouint(((windowobj_t*)w)->text);
   getFont()->padding = padding;
   window_clearbuffer(getSelectedWindow(), getSelectedWindow()->bgcolour);
   gui_redrawall();
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
   window_settings_t *settings = (window_settings_t*)parent->state;
   uint16_t colour = (uint16_t)hextouint(settings->theme_colour_wo->text);
   window_popup_colourpicker(getWindow(popup), parent, &window_settings_set_theme_colour_callback, colour);
   window_draw_outline(getWindow(popup), false);
}

void window_settings_pick_theme_colour2(void *w, void *regs) {
   (void)regs;
   (void)w;
   gui_window_t *parent = getSelectedWindow();
   int popup = windowmgr_add();
   window_settings_t *settings = (window_settings_t*)parent->state;
   uint16_t colour = (uint16_t)hextouint(settings->theme_colour2_wo->text);
   window_popup_colourpicker(getWindow(popup), parent, &window_settings_set_theme_colour2_callback, colour);
   window_draw_outline(getWindow(popup), false);
}

void window_settings_set_font(void *w) {
   fat_dir_t *entry = fat_parse_path(((windowobj_t*)w)->text, true);
   if(entry == NULL) {
      debug_writestr("Font not found\n");
      return;
   }

   fontfile_t *file = (fontfile_t*)fat_read_file(entry->firstClusterNo, entry->fileSize);
   font_load(file);
   free((uint32_t)entry, sizeof(fat_dir_t));
   debug_printf("Selected window %i\n", getSelectedWindowIndex());
   window_settings_redraw(getSelectedWindow());
   gui_redrawall();
}

void window_settings_font_callback(char *path) {
   windowobj_t *wo = ((window_settings_t*)getSelectedWindow()->state)->theme_fontpath_wo;
   strcpy(wo->text, path);
   wo->textpos = strlen(wo->text);
   wo->cursor_textpos =  wo->textpos;
   window_settings_set_font(wo);
}

void window_settings_browesefont(void *w, void *regs) {
   (void)w;
   (void)regs;
   gui_window_t *parent = getSelectedWindow();
   int popup = windowmgr_add();
   window_popup_filepicker(getWindow(popup), parent, &window_settings_font_callback);
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
      window_resize(NULL, window, 365, 300);

      int y = 35;

      char text[256];

      window->scrollable_content_height = 300;
      window_create_scrollbar(window, NULL);

      // desktop background colour
      uinttohexstr(gui_bg, text);
      settings->d_bgcolour_wo = window_create_text(window, x, y, text);
      settings->d_bgcolour_wo->oneline = true;
      settings->d_bgcolour_wo->return_func = &window_settings_set_bgcolour;
      settings->d_bgcolourpick_wo = window_create_button(window, x2, y, "Pick", &window_settings_pickdbgcolour);

      // desktop background image
      y += 25;
      settings->d_bgimg_wo = window_create_text(window, x, y, windowmgr_get_settings()->desktop_bgimg);
      settings->d_bgimg_wo->oneline = true;
      settings->d_bgimg_wo->return_func = &window_settings_set_bgimg;
      settings->d_bgimgpick_wo = window_create_button(window, x2, y, "Browse", &window_settings_browesebg);

      // window background colour
      y += 25;
      uinttohexstr(selected->bgcolour, text);
      settings->w_bgcolour_wo = window_create_text(window, x, y, text);
      settings->w_bgcolour_wo->oneline = true;
      settings->w_bgcolour_wo->return_func = &window_settings_set_window_bgcolour;
      settings->w_bgcolourpick_wo = window_create_button(window, x2, y, "Pick", &window_settings_pickbgcolour);

      // window text colour
      y += 25;
      uinttohexstr(selected->txtcolour, text);
      settings->w_txtcolour_wo = window_create_text(window, x, y, text);
      settings->w_txtcolour_wo->oneline = true;
      settings->w_txtcolour_wo->return_func = &window_settings_set_window_txtcolour;
      settings->w_txtcolourpick_wo = window_create_button(window, x2, y, "Pick", &window_settings_picktxtcolour);

      // text padding
      y += 25;
      uinttostr(getFont()->padding, text);
      settings->theme_txtpadding_wo = window_create_text(window, x, y, text);
      settings->theme_txtpadding_wo->oneline = true;
      settings->theme_txtpadding_wo->return_func = &window_settings_set_txtpadding;

      // theme menu
      y += 25;
      windowobj_menu_t *menuitems = (windowobj_menu_t*)malloc(sizeof(windowobj_menu_t) * 2);
      strcpy(menuitems[0].text, "Classic");
      menuitems[0].func = &window_settings_set_theme_classic;
      menuitems[0].disabled = false;
      strcpy(menuitems[1].text, "Gradient");
      menuitems[1].func = &window_settings_set_theme_gradient;
      menuitems[1].disabled = false;
      settings->theme_wo = window_create_menu(window, x, y, menuitems, 2);
      settings->theme_wo->menuselected = windowmgr_get_settings()->theme; // classic

      // gradient style
      y += 35;
      menuitems = (windowobj_menu_t*)malloc(sizeof(windowobj_menu_t) * 2);
      strcpy(menuitems[0].text, "Horizontal");
      menuitems[0].func = &window_settings_set_theme_gradient_horizontal;
      menuitems[0].disabled = false;
      strcpy(menuitems[1].text, "Vertical");
      menuitems[1].func = &window_settings_set_theme_gradient_vertical;
      menuitems[1].disabled = false;
      settings->theme_gradientstyle_wo = window_create_menu(window, x, y, menuitems, 2);
      settings->theme_gradientstyle_wo->menuselected = windowmgr_get_settings()->titlebar_gradientstyle; // horizontal

      // theme colour 1
      y += 35;
      uinttohexstr(windowmgr_get_settings()->titlebar_colour, text);
      settings->theme_colour_wo = window_create_text(window, x, y, text);
      settings->theme_colour_wo->oneline = true;
      settings->theme_colour_wo->return_func = &window_settings_set_theme_colour;
      settings->theme_colourpick_wo = window_create_button(window, x2, y, "Pick", &window_settings_pick_theme_colour);

      // theme colour 2
      y += 25;
      uinttohexstr(windowmgr_get_settings()->titlebar_colour2, text);
      settings->theme_colour2_wo = window_create_text(window, x, y, text);
      settings->theme_colour2_wo->oneline = true;
      settings->theme_colour2_wo->return_func = &window_settings_set_theme_colour2;
      settings->theme_colour2pick_wo = window_create_button(window, x2, y, "Pick", &window_settings_pick_theme_colour2);

      // font
      y += 25;
      settings->theme_fontpath_wo = window_create_text(window, x, y, windowmgr_get_settings()->font_path);
      settings->theme_fontpath_wo->oneline = true;
      settings->theme_fontpath_wo->return_func = &window_settings_set_font;
      settings->theme_fontpathpick_wo = window_create_button(window, x2, y, "Browse", &window_settings_browesefont);

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
      settings->w_txtcolourpick_wo->oneline = true;
      settings->w_txtcolour_wo->return_func = &window_settings_set_window_txtcolour;
      window_create_button(window, x2, y, "Pick", &window_settings_picktxtcolour);

   }

   window_settings_redraw(window);

   return settings;
}