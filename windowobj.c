#include <stddef.h>

#include "windowobj.h"
#include "draw.h"
#include "windowmgr.h"
#include "tasks.h"
#include "events.h"

// window widgets/objects

void windowobj_init(windowobj_t *windowobj, surface_t *window_surface) {
   windowobj->window_surface = window_surface;

   // default settings
   windowobj->type = WO_BUTTON;
   windowobj->x = 10;
   windowobj->y = 10;
   windowobj->width = 50;
   windowobj->height = 14;
   windowobj->text = NULL;
   windowobj->textpadding = 3;
   windowobj->hovering = false;
   windowobj->clicked = false;
   windowobj->textpos = 0;
   
   // default funcs
   windowobj->draw_func = &windowobj_draw;
   windowobj->click_func = NULL;
   windowobj->hover_func = &windowobj_hover;
}


void windowobj_drawstr(windowobj_t *wo, uint16_t colour) {

   int x = wo->textpadding;
   int y = wo->textpadding;

   for(int i = 0; i < strlen(wo->text); i++) {
      char c = wo->text[i];
      if(c == '\n') {
         y += wo->textpadding + getFont()->height;
         x = wo->textpadding;
      } else {
         draw_char(wo->window_surface, c, colour, wo->x + x, wo->y + y);
         x += wo->textpadding + getFont()->width;
      }

      if(x + wo->textpadding + getFont()->width > wo->width) {
         y += wo->textpadding + getFont()->height;
         x = wo->textpadding;
      }
   }
   //draw_string(wo->window_surface, wo->text, colour, wo->x+wo->textpadding, wo->y+wo->textpadding);

}

void windowobj_draw(void *windowobj) {

   windowobj_t *wo = (windowobj_t*)windowobj;

   if(wo->type == WO_DISABLED) return;
   

   uint16_t bg = rgb16(255, 255, 255);
   uint16_t border = rgb16(120, 120, 120);
   uint16_t text = 0;

   if(wo->type == WO_BUTTON)
      bg = rgb16(180, 180, 180);
   
   if(wo->clicked) {
      if(wo->type == WO_BUTTON) {
         bg = rgb16(160, 160, 160);
         text = rgb16(40, 40, 40);
      }
      border = 0;
   } else if(wo->hovering) {
      bg = rgb16(230, 230, 230);
      if(wo->type == WO_BUTTON) {
         bg = rgb16(200, 200, 200);
      }
      border = rgb16(40, 40, 40);
      text = rgb16(40, 40, 40);
   }
   
   draw_rect(wo->window_surface, bg, wo->x, wo->y, wo->width, wo->height);
   draw_unfilledrect(wo->window_surface, border, wo->x, wo->y, wo->width, wo->height);

   windowobj_drawstr(wo, text);
   
}

void windowobj_redraw(void *window, void *windowobj) {
   if(((gui_window_t*)window)->dragged) return;
   windowobj_t *wo = (windowobj_t*)windowobj;
   window_draw_content_region((gui_window_t*)window, wo->x, wo->y, wo->width, wo->height);
}

void windowobj_unclick(void *regs, void *windowobj) {
   (void)regs;
   windowobj_t *wo = (windowobj_t*)windowobj; 
   wo->clicked = false;
   windowobj_draw(wo);
}

void windowobj_click(void *regs, void *windowobj) {
   windowobj_t *wo = (windowobj_t*)windowobj;

   wo->clicked = true;
   if(wo->type == WO_BUTTON)
      events_add(5, &windowobj_unclick, (void*)wo, -1);

   if(wo->click_func != NULL)
      task_call_subroutine(regs, (uint32_t)(wo->click_func), NULL, 0);
}

void windowobj_hover(void *windowobj) {
   windowobj_t *wo = (windowobj_t*)windowobj; 
   wo->hovering = true;
   windowobj_draw(windowobj);
}

extern char scan_to_char(int scan_code);

void windowobj_keydown(void *windowobj, int scan_code) {
   windowobj_t *wo = (windowobj_t*)windowobj;
   if(wo->type != WO_TEXT || wo->text == NULL) return;

   char c = '\0';

   switch(scan_code) {
      case 28: // return
         c = '\n';
         break;
      case 14: // backspace
         wo->textpos--;
         if(wo->textpos < 0) wo->textpos = 0;
         wo->text[wo->textpos] = '\0';
         windowobj_draw(windowobj);
         break;
      case 72: // up arrow
         break;
      case 80: // down arrow
         break;
      default:
         c = scan_to_char(scan_code);
         break;
   }

   if(c == 0) return;

   wo->text[wo->textpos++] = c;
   wo->text[wo->textpos] = '\0';

   windowobj_draw(windowobj);
}