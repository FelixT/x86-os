#include "windowobj.h"
#include "draw.h"
#include "windowmgr.h"
#include <stddef.h>
#include "tasks.h"

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
   
   // default funcs
   windowobj->draw_func = &windowobj_draw;
   windowobj->click_func = NULL;
   windowobj->hover_func = &windowobj_hover;
}

void windowobj_draw(void *windowobj) {

   windowobj_t *wo = (windowobj_t*)windowobj;

   if(wo->type == WO_DISABLED) return;
   if(wo->hovering) return;

   draw_rect(wo->window_surface, rgb16(180, 180, 180), wo->x, wo->y, wo->width, wo->height);
   draw_unfilledrect(wo->window_surface, rgb16(120, 120, 120), wo->x, wo->y, wo->width, wo->height);

   if(wo->text != NULL)
      draw_string(wo->window_surface, wo->text, 0, wo->x+wo->textpadding, wo->y+wo->textpadding);

}

void windowobj_redraw(void *window, void *windowobj) {
   windowobj_t *wo = (windowobj_t*)windowobj; 
   window_draw_content_region((gui_window_t*)window, wo->x, wo->y, wo->width, wo->height);
}

void windowobj_click(void *regs, void *windowobj) {
   windowobj_t *wo = (windowobj_t*)windowobj; 
   draw_rect(wo->window_surface, rgb16(140, 140, 140), wo->x, wo->y, wo->width, wo->height);
   draw_unfilledrect(wo->window_surface, rgb16(20, 20, 20), wo->x, wo->y, wo->width, wo->height);
   if(wo->text != NULL)
      draw_string(wo->window_surface, wo->text, 0, wo->x+wo->textpadding, wo->y+wo->textpadding);

   if(wo->click_func != NULL)
      task_call_subroutine(regs, (uint32_t)(wo->click_func), NULL, 0);
}

void windowobj_hover(void *windowobj) {
   windowobj_t *wo = (windowobj_t*)windowobj; 
   wo->hovering = true;
   draw_rect(wo->window_surface, rgb16(200, 200, 200), wo->x, wo->y, wo->width, wo->height);
   draw_unfilledrect(wo->window_surface, rgb16(40, 40, 40), wo->x, wo->y, wo->width, wo->height);

   if(wo->text != NULL)
      draw_string(wo->window_surface, wo->text, rgb16(40, 40, 40), wo->x+wo->textpadding, wo->y+wo->textpadding);

}