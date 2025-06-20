#include "../prog.h"
#include "wo_api.h"
#include "../prog_wo.h"
#include "../../lib/string.h"

windowobj_t *create_button(int x, int y, char *text) {
   windowobj_t *wo = register_windowobj(WO_BUTTON, x, y, 50, 14);
   char *newtext = (char*)malloc(256);
   strcpy(newtext, text);
   wo->text = newtext;
   return wo;
}

windowobj_t *create_text(int x, int y, char *text) {
   windowobj_t *wo = register_windowobj(WO_TEXT, x, y, 100, 14);
   char *newtext = (char*)malloc(256);
   strcpy(newtext, text);
   wo->text = newtext;
   return wo;
}