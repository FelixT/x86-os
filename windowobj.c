#include <stddef.h>

#include "windowobj.h"
#include "draw.h"
#include "windowmgr.h"
#include "tasks.h"
#include "events.h"
#include "lib/string.h"

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
   windowobj->textpadding = getFont()->padding;
   windowobj->hovering = false;
   windowobj->clicked = false;
   windowobj->disabled = false;
   windowobj->textpos = 0;
   windowobj->textvalign = true;
   windowobj->texthalign = true;
   windowobj->cursor_textpos = 0;
   windowobj->cursorx = windowobj->textpadding + 1;
   windowobj->cursory = windowobj->textpadding + 1;
   windowobj->menuitem_count = 0;
   windowobj->menuitems = NULL;
   windowobj->menuselected = -1;
   windowobj->visible = true;
   
   // default funcs
   windowobj->draw_func = &windowobj_draw;
   windowobj->click_func = NULL;
   windowobj->hover_func = &windowobj_hover;
   windowobj->return_func = NULL;
}


void windowobj_drawstr(windowobj_t *wo, uint16_t colour) {

   if(wo->text == NULL) return;

   int x = wo->textpadding + 1;
   int y = wo->textpadding + 1;
   if(wo->textvalign)
      y = (wo->height - (getFont()->height + wo->textpadding))/2+1;
   if(wo->texthalign) {
      // get length
      x = (wo->width - font_width(strlen(wo->text)))/2;
   }

   if(wo->cursor_textpos == 0) {
      wo->cursorx = x;
      wo->cursory = y + getFont()->height - 2;
   }

   // draw all characers of string... no scrolling implemented yet
   for(int i = 0; i < strlen(wo->text); i++) {
      char c = wo->text[i];
      if(c == '\n') {
         y += wo->textpadding + getFont()->height;
         x = wo->textpadding + 1;
      } else {
         draw_char(wo->window_surface, c, colour, wo->x + x, wo->y + y);
         x += wo->textpadding + getFont()->width;

         // wrapping
         if(x + wo->textpadding + getFont()->width > wo->width) {
            y += wo->textpadding + getFont()->height;
            x = wo->textpadding + 1;
         }
      }

      if(i+1 == wo->cursor_textpos) {
         wo->cursorx = x;
         wo->cursory = y + getFont()->height - 2;
      }

   }
}

extern bool mouse_held;
void windowobj_draw(void *windowobj) {

   windowobj_t *wo = (windowobj_t*)windowobj;

   if(wo->type == WO_DISABLED) return;
   if(wo->visible == false) return;
   
   uint16_t bg = rgb16(255, 255, 255);
   uint16_t border = rgb16(120, 120, 120);
   uint16_t text = 0;
   uint16_t light = rgb16(235, 235, 235);
   uint16_t dark = rgb16(145, 145, 145);

   if(wo->type == WO_BUTTON) {
      bg = rgb16(200, 200, 200);

      wo->clicked = wo->clicked && mouse_held;
      if(wo->clicked) {
         bg = rgb16(185, 185, 185);
         text = rgb16(40, 40, 40);
      } else {
         if(wo->hovering)
            bg = rgb16(220, 220, 220);

         uint16_t tmp = light;
         light = dark;
         dark = tmp;
      }

      draw_rect(wo->window_surface, bg, wo->x, wo->y, wo->width, wo->height);

      draw_line(wo->window_surface, dark,  wo->x, wo->y, true,  wo->height);
      draw_line(wo->window_surface, dark,  wo->x, wo->y, false, wo->width);
      draw_line(wo->window_surface, light, wo->x, wo->y + wo->height - 1, false, wo->width);
      draw_line(wo->window_surface, light, wo->x + wo->width - 1, wo->y, true,  wo->height);


   } else if(wo->type == WO_TEXT) {

      if(wo->clicked && !wo->disabled) {
         dark = rgb16(40, 40, 40);
         light = rgb16(140, 140, 140);
      } else if(wo->hovering && !wo->disabled) {
         bg = rgb16(245, 245, 245);
         border = rgb16(40, 40, 40);
         text = rgb16(40, 40, 40);
      }

      draw_rect(wo->window_surface, bg, wo->x + 1, wo->y + 1, wo->width - 2, wo->height - 2);
      draw_line(wo->window_surface, dark,  wo->x, wo->y, false, wo->width);
      draw_line(wo->window_surface, dark,  wo->x, wo->y, true,  wo->height);
      draw_line(wo->window_surface, light, wo->x, wo->y + wo->height - 1, false, wo->width);
      draw_line(wo->window_surface, light, wo->x + wo->width - 1, wo->y, true, wo->height);
   } else if(wo->type == WO_MENU) {
      if(wo->clicked) {
         border = 0;
      } else if(wo->hovering) {
         bg = rgb16(240, 240, 240);
         border = rgb16(40, 40, 40);
         text = rgb16(40, 40, 40);
      }

      draw_rect(wo->window_surface, bg, wo->x+1, wo->y+1, wo->width-2, wo->height-2);
      if(wo->menuselected >= 0) {
         // draw selected item
         draw_rect(wo->window_surface, rgb16(200, 200, 200), wo->x + 1, wo->y + 1 + (wo->menuselected * (getFont()->height + 4)), wo->width - 2, getFont()->height + 4);
      }
      draw_unfilledrect(wo->window_surface, border, wo->x, wo->y, wo->width, wo->height);

      // draw menu items
      for(int i = 0; i < wo->menuitem_count; i++) {
         windowobj_menu_t *item = &wo->menuitems[i];
         draw_string(wo->window_surface, item->text, text, wo->x + 4, wo->y + 4 + (i * (getFont()->height + 4)));
      }
   } else {
      if(wo->clicked) {
         border = 0;
      } else if(wo->hovering) {
         bg = rgb16(240, 240, 240);
         border = rgb16(40, 40, 40);
         text = rgb16(40, 40, 40);
      }

      draw_rect(wo->window_surface, bg, wo->x, wo->y, wo->width, wo->height);
      draw_unfilledrect(wo->window_surface, border, wo->x, wo->y, wo->width, wo->height);

   }

   if(wo->type == WO_BUTTON && !wo->clicked) {
      wo->y++;
      windowobj_drawstr(wo, rgb16(215, 215, 215));
      wo->y--;
   }
   windowobj_drawstr(wo, text);

   if(wo->type == WO_TEXT && wo->clicked) {
      // draw cursor
      draw_rect(wo->window_surface, text, wo->x + wo->cursorx, wo->y + wo->cursory, getFont()->width, 2);
   }
   
}

void windowobj_redraw(void *window, void *windowobj) {
   gui_window_t *w = (gui_window_t*)window;
   if(w->dragged || w->resized) return;
   windowobj_t *wo = (windowobj_t*)windowobj;
   window_draw_content_region((gui_window_t*)window, wo->x, wo->y, wo->width, wo->height);
}

typedef struct {
   windowobj_t *wo;
   gui_window_t *window;
} windowobj_click_event_t;

extern int gui_mouse_x;
extern int gui_mouse_y;
extern bool gui_cursor_shown;
void windowobj_unclick(void *regs, void *event) {
   (void)regs;
   windowobj_click_event_t *e = (windowobj_click_event_t*)event;
   if(!e->window->active) return;
   e->wo->clicked = false;
   windowobj_draw(e->wo);
   window_draw_content_region(e->window, e->wo->x, e->wo->y, e->wo->width, e->wo->height + getFont()->height);
   free((uint32_t)event, sizeof(windowobj_click_event_t));
}

void windowobj_click(void *regs, void *windowobj) {
   windowobj_t *wo = (windowobj_t*)windowobj;

   if(wo->disabled) return;
   
   wo->clicked = true;
   if(wo->type == WO_BUTTON) {
      windowobj_click_event_t *event = (windowobj_click_event_t*)malloc(sizeof(windowobj_click_event_t));
      event->wo = wo;
      event->window = getSelectedWindow();
      events_add(30, &windowobj_unclick, (void*)event, -1);
   }

   if(wo->type == WO_MENU) {
      if(wo->menuselected >= 0) {
         windowobj_menu_t *item = &wo->menuitems[wo->menuselected];
         if(item->func != NULL)
            item->func();
      }
   }

   if(wo->click_func != NULL) {
      if(get_task_from_window(getSelectedWindowIndex()) == -1) {
         // kernel
         // supply with regs so we can switch task
         ((void (*)(void*, void*))wo->click_func)(windowobj, regs);
      } else {
         gui_interrupt_switchtask(regs);
         task_call_subroutine(regs, "woclick", (uint32_t)(wo->click_func), NULL, 0);
      }
   }
}

void windowobj_hover(void *windowobj, int x, int y) {
   (void)x;
   windowobj_t *wo = (windowobj_t*)windowobj;
   bool hovering = wo->hovering;
   bool redraw = false;
   if(wo->type == WO_MENU) {
      int prevselected = wo->menuselected;
      wo->menuselected = y / (getFont()->height + 4);
      if(wo->menuselected < 0 || wo->menuselected >= wo->menuitem_count)
         wo->menuselected = -1;

      if(prevselected != wo->menuselected)
         redraw = true;
   }

   if(!hovering) {
      wo->hovering = true;
      redraw = true;
   }

   if(redraw)
      windowobj_draw(wo);

}

extern char scan_to_char(int scan_code, bool caps);
extern bool keyboard_shift;
extern bool keyboard_caps;

void windowobj_move_cursor_vertical(windowobj_t *wo, int direction) {
    // direction: -1 for up, 1 for down
    
    int line_starts[100];
    int line_count = 1;
    line_starts[0] = 0;
    
    int x = wo->textpadding + 1;
    for(int i = 0; i < wo->textpos; i++) {
      if(wo->text[i] == '\n') {
         line_starts[line_count++] = i + 1;
         x = wo->textpadding + 1;
      } else {
         x += wo->textpadding + getFont()->width;
         if(x + wo->textpadding + getFont()->width > wo->width) {
            line_starts[line_count++] = i + 1;
            x = wo->textpadding + 1;
         }
      }
   }
   int current_line = 0;
   for(int i = 0; i < line_count; i++) {
      if(wo->cursor_textpos >= line_starts[i])
         current_line = i;
   }
   int target_line = current_line + direction;
   if(target_line < 0 || target_line >= line_count)
      return;
    
   int offset_x = wo->cursor_textpos - line_starts[current_line];
   int target_line_start = line_starts[target_line];
   int target_line_end = (target_line + 1 < line_count) ? line_starts[target_line + 1] - 1 : wo->textpos;
    
   if(target_line_end > target_line_start && wo->text[target_line_end - 1] == '\n')
      target_line_end--;
    
   int target_line_length = target_line_end - target_line_start;
   if(offset_x > target_line_length) {
      offset_x = target_line_length;
   }
    
   wo->cursor_textpos = target_line_start + offset_x;
   windowobj_draw(wo);
}

void windowobj_keydown(void *regs, void *windowobj, int scan_code) {
   windowobj_t *wo = (windowobj_t*)windowobj;

   if(wo->disabled) return;

   if(wo->type == WO_MENU) {
      // handle uparrow/downarrow
      if(scan_code == 72) {
         if(wo->menuselected > 0) wo->menuselected--;
      } else if(scan_code == 80) {
         if(wo->menuselected < wo->menuitem_count-1) wo->menuselected++;
      }
      windowobj_draw(wo);
      return;
   }

   if(wo->type != WO_TEXT || wo->text == NULL) return;

   char c = 0;

   switch(scan_code) {
      case 28: // return
         if(wo->return_func != NULL) {
            gui_interrupt_switchtask(regs);
            if(get_task_from_window(getSelectedWindowIndex()) == -1) // kernel
               wo->return_func(wo);
            else // usr
               task_call_subroutine(regs, "woreturn", (uint32_t)(wo->return_func), NULL, 0);
         } else {
            c = '\n';
         }
         break;
      case 14: // backspace
         if(wo->cursor_textpos > 0 && wo->textpos > 0) {
            for(int i = wo->cursor_textpos - 1; i < wo->textpos - 1; i++) {
               wo->text[i] = wo->text[i + 1];
            }
            wo->textpos--;
            wo->cursor_textpos--;
            wo->text[wo->textpos] = '\0';
            windowobj_draw(windowobj);
         }
         break;
      case 72: // up arrow
         windowobj_move_cursor_vertical(wo, -1);
         break;
      case 80: // down arrow
         windowobj_move_cursor_vertical(wo, 1);
         break;
      case 75: // left arrow
         if(wo->cursor_textpos > 0) {
            wo->cursor_textpos--;
         }
         windowobj_draw(wo);
         break;
      case 77: // right arrow
         if(wo->cursor_textpos < wo->textpos) {
            wo->cursor_textpos++;
         }
         windowobj_draw(wo);
         break;
      case 0x3A:
         keyboard_caps = !keyboard_caps;
         break;   
      default:
         c = scan_to_char(scan_code, keyboard_shift^keyboard_caps);
         break;
   }

   if(c == 0) return;

   for(int i = wo->textpos; i > wo->cursor_textpos; i--) {
      wo->text[i] = wo->text[i - 1];
   }

   wo->text[wo->cursor_textpos] = c;
   wo->textpos++;
   wo->text[wo->textpos] = '\0';

   if(c == '\n') {
      // cover current cursor
      draw_rect(wo->window_surface, 0xFFFF, wo->x + wo->cursorx, wo->y + wo->cursory, getFont()->width, 2);
      wo->cursory += getFont()->height + wo->textpadding;
      wo->cursorx = wo->textpadding + 1;
      wo->cursor_textpos++;
   } else {
      draw_rect(wo->window_surface, rgb16(255, 255, 255), wo->x + wo->cursorx, wo->y + wo->cursory - getFont()->height + 2, getFont()->width, getFont()->height);
      draw_char(wo->window_surface, c, 0, wo->x + wo->cursorx, wo->y + wo->cursory - getFont()->height + 2);
      wo->cursorx += getFont()->width + wo->textpadding;
      wo->cursor_textpos++;
      if(wo->cursorx + getFont()->width + wo->textpadding > wo->width) {
         wo->cursory += getFont()->height + wo->textpadding;
         wo->cursorx = wo->textpadding + 1;
      }
   }

   // draw cursor
   draw_rect(wo->window_surface, 0, wo->x + wo->cursorx, wo->y + wo->cursory, getFont()->width, 2);
}