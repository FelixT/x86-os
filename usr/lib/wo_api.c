#include "../prog.h"
#include "wo_api.h"
#include "../prog_wo.h"
#include "../../lib/string.h"

windowobj_t *create_button(windowobj_t *parent, int x, int y, char *text) {
   windowobj_t *wo;
   if(parent == NULL) {
      wo = register_windowobj(WO_BUTTON, x, y, 50, 14);
   } else {
      wo = windowobj_add_child(parent, WO_BUTTON, x, y, 50, 14);
   }
   char *newtext = (char*)malloc(256);
   strcpy(newtext, text);
   wo->text = newtext;
   return wo;
}

windowobj_t *create_text(windowobj_t *parent, int x, int y, char *text) {
   windowobj_t *wo;
   if(parent == NULL) {
      wo = register_windowobj(WO_TEXT, x, y, 100, 14);
   } else {
      wo = windowobj_add_child(parent, WO_TEXT, x, y, 100, 14);
   }
   char *newtext = (char*)malloc(256);
   strcpy(newtext, text);
   wo->text = newtext;
   return wo;
}

void set_text(windowobj_t *wo, char *text) {
   strcpy(wo->text, text);
   wo->textpos = strlen(text);
   wo->cursor_textpos = wo->textpos;
}