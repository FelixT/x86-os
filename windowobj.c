#include <stddef.h>

#include "windowobj.h"
#include "draw.h"
#include "windowmgr.h"
#include "tasks.h"
#include "events.h"

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
   
   // default funcs
   windowobj->draw_func = &windowobj_draw;
   windowobj->click_func = NULL;
   windowobj->hover_func = &windowobj_hover;
}

void windowobj_draw(void *windowobj) {

   windowobj_t *wo = (windowobj_t*)windowobj;

   if(wo->type == WO_DISABLED) return;
   
   uint16_t bg = rgb16(180, 180, 180);
   uint16_t border = rgb16(120, 120, 120);
   uint16_t text = 0;

   if(wo->clicked) {
      bg = rgb16(160, 160, 160);
      border = 0;
      text = rgb16(40, 40, 40);
   } else if(wo->hovering) {
      bg = rgb16(200, 200, 200);
      border = rgb16(40, 40, 40);
      text = rgb16(40, 40, 40);
   }
   
   draw_rect(wo->window_surface, bg, wo->x, wo->y, wo->width, wo->height);
   draw_unfilledrect(wo->window_surface, border, wo->x, wo->y, wo->width, wo->height);

   if(wo->text != NULL)
      draw_string(wo->window_surface, wo->text, text, wo->x+wo->textpadding, wo->y+wo->textpadding);

   
}

void windowobj_redraw(void *window, void *windowobj) {
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
   events_add(5, &windowobj_unclick, (void*)wo, -1);

   if(wo->click_func != NULL)
      task_call_subroutine(regs, (uint32_t)(wo->click_func), NULL, 0);
}

void windowobj_hover(void *windowobj) {
   windowobj_t *wo = (windowobj_t*)windowobj; 
   wo->hovering = true;
   windowobj_draw(windowobj);
}