#include "../prog.h"
#include "wo_api.h"
#include "../prog_wo.h"
#include "../../lib/string.h"

int text_width(windowobj_t *wo) {
   if(!wo || !wo->text) return 0;
   return strlen(wo->text)*(get_font_info().width+wo->textpadding);
}

windowobj_t *create_wo_w(int window, windowobj_t *parent, int type, int x, int y, int width, int height) {
   windowobj_t *wo;
   if(parent == NULL) {
      wo = register_windowobj(window, type, x, y, width, height);
   } else {
      wo = windowobj_add_child(parent, type, x, y, width, height);
   }
   return wo;
}

windowobj_t *create_wo(windowobj_t *parent, int type, int x, int y, int width, int height) {
   return create_wo_w(-1, parent, type, x, y, width, height);
}

windowobj_t *create_canvas_w(int window, windowobj_t *parent, int x, int y, int width, int height) {
   windowobj_t *wo = create_wo_w(window, parent, WO_CANVAS, x, y, width, height);
   return wo;
}

windowobj_t *create_canvas(windowobj_t *parent, int x, int y, int width, int height) {
   return create_canvas_w(-1, parent, x, y, width, height);
}

windowobj_t *create_button_w(int window, windowobj_t *parent, int x, int y, char *text) {
   windowobj_t *wo = create_wo_w(window, parent, WO_BUTTON, x, y, 50, 14);
   char *newtext = (char*)malloc(256);
   strcpy(newtext, text);
   wo->text = newtext;
   wo->width = text_width(wo) + 10;
   return wo;
}

windowobj_t *create_button(windowobj_t *parent, int x, int y, char *text) {
   return create_button_w(-1, parent, x, y, text);
}

windowobj_t *create_text_w(int window, windowobj_t *parent, int x, int y, char *text) {
   windowobj_t *wo = create_wo_w(window, parent, WO_TEXT, x, y, 100, 14);
   char *newtext = (char*)malloc(256);
   strcpy(newtext, text);
   wo->text = newtext;
   return wo;
}

windowobj_t *create_text(windowobj_t *parent, int x, int y, char *text) {
   return create_text_w(-1, parent, x, y, text);
}

windowobj_t *create_text_static_w(int window, windowobj_t *parent, int x, int y, char *text) {
   windowobj_t *wo = create_wo_w(window, parent, WO_TEXT, x, y, 100, 14);
   char *newtext = (char*)malloc(256);
   strcpy(newtext, text);
   wo->text = newtext;
   wo->bordered = false;
   wo->width = text_width(wo) + 10;
   wo->isstatic = true;
   return wo;
}

windowobj_t *create_text_static(windowobj_t *parent, int x, int y, char *text) {
   return create_text_static_w(-1, parent, x, y, text);
}

void set_text(windowobj_t *wo, char *text) {
   strcpy(wo->text, text);
   wo->textpos = strlen(text);
   wo->cursor_textpos = wo->textpos;
}
