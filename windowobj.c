#include <stddef.h>

#include "windowobj.h"
#include "draw.h"
#include "windowmgr.h"
#include "tasks.h"
#include "events.h"
#include "lib/string.h"
#include "lib/maths.h"
#include "window.h"

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
   windowobj->isstatic = false;
   windowobj->textpos = 0;
   windowobj->textvalign = true;
   windowobj->texthalign = true;
   windowobj->underline = false;
   windowobj->cursor_textpos = 0;
   windowobj->cursorx = windowobj->textpadding + 1;
   windowobj->cursory = windowobj->textpadding + 1;
   windowobj->menuitem_count = 0;
   windowobj->menuitems = NULL;
   windowobj->menuhovered = -1;
   windowobj->menuselected = -1;
   windowobj->visible = true;
   windowobj->child_count = 0;
   windowobj->parent = NULL;
   windowobj->oneline = false;
   windowobj->bordered = true;
   windowobj->colour_bg = COLOUR_WHITE;
   windowobj->colour_text = COLOUR_BLACK;
   windowobj->colour_border = rgb16(120, 120, 120);
   windowobj->colour_bg_hover = rgb16(247, 247, 247);
   windowobj->colour_border_hover = rgb16(40, 40, 40);

   // default funcs
   windowobj->draw_func = &windowobj_draw;
   windowobj->click_func = NULL;
   windowobj->hover_func = &windowobj_hover;
   windowobj->return_func = NULL;
   windowobj->release_func = NULL;
   windowobj->drag_func = NULL;
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

   // draw characters, skip off screen
   for(int i = 0; i < strlen(wo->text); i++) {
      char c = wo->text[i];
      if(c == '\n') {
         y += wo->textpadding + getFont()->height;
         x = wo->textpadding + 1;
      } else if(c == '\t') {
         // draw 4 spaces
         for(int s = 0; s < 4; s++) {
            if(wo->y + y + getFont()->height >= 0 && wo->y + y <= wo->window_surface->height) {
               draw_char(wo->window_surface, ' ', colour, wo->x + x, wo->y + y);
               if(wo->underline)
                  draw_line(wo->window_surface, colour, wo->x + x - getFont()->padding/2, wo->y + y + getFont()->height + 2, false, getFont()->width+getFont()->padding);
            }
            x += wo->textpadding + getFont()->width;

            // wrapping
            if(x + wo->textpadding + getFont()->width > wo->width) {
               y += wo->textpadding + getFont()->height;
               x = wo->textpadding + 1;
            }
         }
      } else {
         if(wo->y + y + getFont()->height >= 0 && wo->y + y <= wo->window_surface->height) {
            draw_char(wo->window_surface, c, colour, wo->x + x, wo->y + y);
            if(wo->underline)
                  draw_line(wo->window_surface, colour, wo->x + x - getFont()->padding/2, wo->y + y + getFont()->height + 2, false, getFont()->width+getFont()->padding);
         }
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

static int windowobj_menu_item_y(int i) {
   return 4 + (getFont()->height + getFont()->padding + 4) * i;
}

void windowobj_menu_resize(windowobj_t *wo) {
   wo->height = windowobj_menu_item_y(wo->menuitem_count) + 4;
   int width = max(60, wo->width);
   for(int i = 0; i < wo->menuitem_count; i++) {
      width = max(width, strlen(wo->menuitems[i].text)*(getFont()->width+getFont()->padding) + 12);
   }
   wo->width = width;
}

static int windowobj_menu_get_item(windowobj_t *wo, int y) {
   y -= 4;
   int i = y/(getFont()->height + getFont()->padding + 4);
   if(i >= wo->menuitem_count) return -1;
   if(i < 0) return -1;
   return i;
}

extern bool mouse_held;
void windowobj_draw(void *windowobj) {

   windowobj_t *wo = (windowobj_t*)windowobj;
   if(wo == NULL) return;

   windowobj_t *parent = (windowobj_t*)wo->parent;

   if(wo->type == WO_DISABLED) return;
   if(!wo->visible || (parent != NULL && !parent->visible)) return;

   // draw relative to parent
   if(parent != NULL) {
      wo->x += parent->x;
      wo->y += parent->y;
   }

   extern surface_t surface;
   bool cursor_hidden = wo->window_surface == &surface
      && cursor_hide_region(wo->x, wo->y, wo->width, wo->height);

   uint16_t bg = wo->colour_bg;
   uint16_t border = wo->colour_border;
   uint16_t text = wo->colour_text;
   uint16_t light = rgb16(235, 235, 235);
   uint16_t dark = rgb16(145, 145, 145);

   if(wo->type == WO_BUTTON || wo->type == WO_SCROLLER) {
      windowmgr_settings_t *wm_settings = windowmgr_get_settings();
      bg = wm_settings->titlebar_colour;

      wo->clicked = wo->clicked && mouse_held;
      bool draw_clicked = wo->clicked && (wo->type == WO_SCROLLER || wo->hovering) && !wo->isstatic;
      if(draw_clicked) {
         bg = rgb16(185, 185, 185);
         text = rgb16(40, 40, 40);
      } else {
         if(wo->hovering)
            bg = rgb16(224, 224, 224);

         uint16_t tmp = light;
         light = dark;
         dark = tmp;
      }

      if(wm_settings->theme == 1) {
         // gradient button
         if(wo->hovering)
            draw_rect_gradient(wo->window_surface, wm_settings->titlebar_colour2, wm_settings->titlebar_colour, wo->x, wo->y, wo->width, wo->height, wm_settings->titlebar_gradientstyle);
         else
            draw_rect_gradient(wo->window_surface, wm_settings->titlebar_colour, wm_settings->titlebar_colour2, wo->x, wo->y, wo->width, wo->height, wm_settings->titlebar_gradientstyle);
      } else {
         draw_rect(wo->window_surface, bg, wo->x, wo->y, wo->width, wo->height);
      }

      if(draw_clicked) {
         wo->y++;
         windowobj_drawstr(wo, rgb16(200, 200, 200));
         wo->y--;
      }

      if(wo->bordered) {
         draw_line(wo->window_surface, dark,  wo->x, wo->y, true,  wo->height);
         draw_line(wo->window_surface, dark,  wo->x, wo->y, false, wo->width);
         draw_line(wo->window_surface, light, wo->x, wo->y + wo->height - 1, false, wo->width);
         draw_line(wo->window_surface, light, wo->x + wo->width - 1, wo->y, true,  wo->height);
      }

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
      if(wo->bordered) {
         draw_line(wo->window_surface, dark,  wo->x, wo->y, false, wo->width);
         draw_line(wo->window_surface, dark,  wo->x, wo->y, true,  wo->height);
         draw_line(wo->window_surface, light, wo->x, wo->y + wo->height - 1, false, wo->width);
         draw_line(wo->window_surface, light, wo->x + wo->width - 1, wo->y, true, wo->height);
      } else {
         draw_line(wo->window_surface, bg,  wo->x, wo->y, false, wo->width);
         draw_line(wo->window_surface, bg,  wo->x, wo->y, true,  wo->height);
      }
   } else if(wo->type == WO_MENU) {
      uint16_t light = rgb16(235, 235, 235);
      uint16_t dark = rgb16(150, 150, 150);

      bg = rgb16(234, 234, 234);
      if(wo->clicked) {
         light = rgb16(80, 80, 80);
         dark = 0;
      } else if(wo->hovering) {
         bg = rgb16(240, 240, 240);
         light = rgb16(200, 200, 200);
         dark = rgb16(160, 160, 160);
         text = rgb16(20, 20, 20);
      }

      draw_rect(wo->window_surface, bg, wo->x+1, wo->y+1, wo->width-2, wo->height-2);

      
      if(wo->menuselected >= 0) {
         // draw selected item
         draw_rect(wo->window_surface, rgb16(200, 200, 200), wo->x + 1, wo->y + windowobj_menu_item_y(wo->menuselected), wo->width - 2, getFont()->height + getFont()->padding + 3);
      }
      if(wo->hovering && wo->menuhovered >= 0 && wo->menuselected != wo->menuhovered) {
         // draw hovered item
         draw_rect(wo->window_surface, rgb16(230, 230, 230), wo->x + 1, wo->y + windowobj_menu_item_y(wo->menuhovered), wo->width - 2, getFont()->height + getFont()->padding + 3);
      }
      if(wo->bordered) {
         draw_line(wo->window_surface, light,  wo->x, wo->y, true,  wo->height);
         draw_line(wo->window_surface, light,  wo->x, wo->y, false, wo->width);
         draw_line(wo->window_surface, dark, wo->x, wo->y + wo->height - 1, false, wo->width);
         draw_line(wo->window_surface, dark, wo->x + wo->width - 1, wo->y, true,  wo->height);
      }
      // draw menu items
      for(int i = 0; i < wo->menuitem_count; i++) {
         windowobj_menu_t *item = &wo->menuitems[i];
         if(strlen(item->text) == 0) // seperator
            draw_line(wo->window_surface, rgb16(200, 200, 200), wo->x + 8, wo->y + windowobj_menu_item_y(i) + (getFont()->height+getFont()->padding)/2 + 2, false, wo->width - 16);
         else
            draw_string(wo->window_surface, item->text, text, wo->x + 8, wo->y + windowobj_menu_item_y(i) + 2);
      }
   } else if(wo->type == WO_SCROLLBAR) {
      bg = rgb16(244, 244, 244);
      if(wo->hovering)
         bg = rgb16(234, 234, 234);
      draw_rect(wo->window_surface, bg, wo->x, wo->y, wo->width, wo->height);
   } else {
      if(wo->clicked) {
         border = 0;
      } else if(wo->hovering) {
         bg = wo->colour_bg_hover;
         border = wo->colour_border_hover;;
         text = rgb16(40, 40, 40);
      }

      draw_rect(wo->window_surface, bg, wo->x, wo->y, wo->width, wo->height);
      if(wo->bordered)
         draw_unfilledrect(wo->window_surface, border, wo->x, wo->y, wo->width, wo->height);
   }

   if((wo->type == WO_BUTTON || wo->type == WO_SCROLLER) && !wo->clicked) {
      wo->y++;
      windowobj_drawstr(wo, rgb16(215, 215, 215));
      wo->y--;
   }
   windowobj_drawstr(wo, text);

   if(wo->type == WO_TEXT && wo->clicked && !wo->isstatic) {
      // draw cursor
      draw_rect(wo->window_surface, text, wo->x + wo->cursorx, wo->y + wo->cursory, getFont()->width, 2);
   }
   
   // draw children
   for(int i = 0; i < wo->child_count; i++) {
      // update x,y to be relative to this windowobj
      windowobj_t *child = (windowobj_t*)wo->children[i];
      windowobj_draw(child);
   }

   cursor_show(cursor_hidden);

   if(wo->parent != NULL) {
      wo->x -= ((windowobj_t*)wo->parent)->x;
      wo->y -= ((windowobj_t*)wo->parent)->y;
   }
}

void windowobj_redraw(void *window, void *windowobj) {
   gui_window_t *w = (gui_window_t*)window;
   if(w->resized) return;
   windowobj_t *wo = (windowobj_t*)windowobj;
   window_draw_content_region(w, wo->x < 0 ? 0 : wo->x, wo->y < 0 ? 0 : wo->y, wo->width, wo->height, true);
}

extern int gui_mouse_x;
extern int gui_mouse_y;
extern bool gui_cursor_shown;

bool windowobj_release(void *windowobj, int relX, int relY) {
   // unclick
   windowobj_t *wo = (windowobj_t*)windowobj;

   if(wo->type != WO_TEXT && wo->type != WO_CANVAS) {
      wo->clicked = false;
      windowobj_draw(wo);
      //window_draw_content_region(getSelectedWindow(), wo->x, wo->y, wo->width, wo->height + getFont()->height);
   }

   if(!wo->visible || (wo->parent != NULL && !((windowobj_t*)wo->parent)->visible)) return false;

   // check if we clicked on a child
   for(int i = 0; i < wo->child_count; i++) {
      windowobj_t *child = (windowobj_t*)wo->children[i];
      if(child->clicked) {
         windowobj_release((void*)child, relX - child->x, relY - child->y);
         return true; // stop at first clicked
      }
   }

   if(wo->type != WO_SCROLLER && !(relX >= 0 && relX < wo->width
   && relY >= 0 && relY < wo->height)) return false;
   
   // clicked inside the object

   if(wo->release_func != NULL) {
      wo->release_func(windowobj, relX, relY);
   }
   return true;
}

extern windowobj_t *default_menu;
void windowobj_click(void *windowobj, int relX, int relY) {
   windowobj_t *wo = (windowobj_t*)windowobj;

   if(wo->disabled) return;
   if(!wo->visible || (wo->parent != NULL && !((windowobj_t*)wo->parent)->visible)) return;
   
   wo->clicked = true;

   if(wo->type == WO_MENU) {
      wo->menuselected = wo->menuhovered;
      if(wo->menuhovered >= 0) {
         windowobj_menu_t *item = &wo->menuitems[wo->menuhovered];
         if(item->func != NULL) {
            item->func();
         }
      }
   }

   // check if we clicked on a child
   int clicked_child = -1;
   for(int i = 0; i < wo->child_count; i++) {
      windowobj_t *child = (windowobj_t*)wo->children[i];
      if(relX >= child->x && relX < child->x + child->width
      && relY >= child->y && relY < child->y + child->height
      && child->visible) {
         windowobj_click((void*)child, relX - child->x, relY - child->y);
         clicked_child = i;
         break; // stop at first child clicked
      }
   }
   // set all others to unclicked
   for(int i = 0; i < wo->child_count; i++) {
      if(i == clicked_child) continue;
      windowobj_t *child = (windowobj_t*)wo->children[i];
      child->clicked = false;
   }

   if(wo->click_func != NULL) {
      wo->click_func(windowobj);
   }
}

void windowobj_hover(void *windowobj, int x, int y) {
   (void)x;
   windowobj_t *wo = (windowobj_t*)windowobj;
   if(!wo->visible || (wo->parent != NULL && !((windowobj_t*)wo->parent)->visible)) return;

   bool hovering = wo->hovering;
   bool redraw = false;
   if(wo->type == WO_MENU) {
      int prevhover = wo->menuhovered;
      wo->menuhovered = windowobj_menu_get_item(wo, y);

      if(prevhover != wo->menuhovered)
         redraw = true;
   }

   if(!hovering) {
      wo->hovering = true;
      redraw = true;
   }

   for(int i = 0; i < wo->child_count; i++) {
      windowobj_t *child = (windowobj_t*)wo->children[i];
      if(x >= child->x && x < child->x + child->width
      && y >= child->y && y < child->y + child->height
      && child->visible) {
         // hover over child
         if(child->hover_func != NULL)
            child->hover_func(child, x - child->x, y - child->y);
         redraw = true;
      } else {
         if(child->hovering) {
            child->hovering = false;
            redraw = true;
         }
      }
   }

   if(redraw)
      windowobj_draw(wo);

}

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

void windowobj_keydown(void *windowobj, uint16_t c) {
   windowobj_t *wo = (windowobj_t*)windowobj;

   if(wo->disabled || wo->isstatic) return;

   // call keydown on all clicked children
   for(int i = 0; i < wo->child_count; i++) {
      windowobj_t *child = wo->children[i];
      if(!child->clicked) continue;

      windowobj_keydown(child, c);
   }

   if(wo->type == WO_MENU) {
      // handle uparrow/downarrow
      if(c == KEY_UPARROW) {
         if(wo->menuhovered > 0) wo->menuhovered--;
      } else if(c == KEY_DOWNARROW) {
         if(wo->menuhovered < wo->menuitem_count-1) wo->menuhovered++;
      }
      windowobj_draw(wo);
      return;
   }

   if(wo->type != WO_TEXT || wo->text == NULL) return;

   switch(c) {
      case KEY_RETURN:
         if(wo->return_func != NULL) {
            wo->return_func(wo);

            if(!wo->oneline) // still do default behaviour
               c = '\n';
         } else {
            c = '\n';
         }
         if(wo->oneline) {
            c = 0;
         }
         break;
      case KEY_BACKSPACE:
         if(wo->cursor_textpos > 0 && wo->textpos > 0) {
            for(int i = wo->cursor_textpos - 1; i < wo->textpos - 1; i++) {
               wo->text[i] = wo->text[i + 1];
            }
            wo->textpos--;
            wo->cursor_textpos--;
            wo->text[wo->textpos] = '\0';
            windowobj_draw(windowobj);
         }
         c = 0;
         break;
      case KEY_UPARROW:
         windowobj_move_cursor_vertical(wo, -1);
         c = 0;
         break;
      case KEY_DOWNARROW:
         windowobj_move_cursor_vertical(wo, 1);
         c = 0;
         break;
      case KEY_LEFTARROW:
         if(wo->cursor_textpos > 0)
            wo->cursor_textpos--;
         windowobj_draw(wo);
         c = 0;
         break;
      case KEY_RIGHTARROW:
         if(wo->cursor_textpos < wo->textpos)
            wo->cursor_textpos++;
         windowobj_draw(wo);
         c = 0;
         break;
      case KEY_SHIFT:
      case KEY_CONTROL:
      case KEY_ESCAPE:
         c = 0;
         break;
   }

   if(c == 0) return;

   for(int i = wo->textpos; i > wo->cursor_textpos; i--) {
      wo->text[i] = wo->text[i - 1];
   }

   wo->text[wo->cursor_textpos] = c;
   wo->textpos++;
   wo->text[wo->textpos] = '\0';

   int offsetX = (wo->parent != NULL ? ((windowobj_t*)wo->parent)->x : 0);
   int offsetY = (wo->parent != NULL ? ((windowobj_t*)wo->parent)->y : 0);

   if(c == '\n') {
      // cover current cursor
      draw_rect(wo->window_surface, 0xFFFF, wo->x + wo->cursorx + offsetX, wo->y + wo->cursory + offsetY, getFont()->width, 2);
      wo->cursory += getFont()->height + wo->textpadding;
      wo->cursorx = wo->textpadding + 1;
      wo->cursor_textpos++;
   } else {
      draw_rect(wo->window_surface, rgb16(255, 255, 255), wo->x + wo->cursorx + offsetX, wo->y + wo->cursory - getFont()->height + 2 + offsetY, getFont()->width, getFont()->height);
      draw_char(wo->window_surface, c, 0, wo->x + wo->cursorx + offsetX, wo->y + wo->cursory - getFont()->height + 2 + offsetY);
      wo->cursorx += getFont()->width + wo->textpadding;
      wo->cursor_textpos++;
      if(wo->cursorx + getFont()->width + wo->textpadding > wo->width) {
         wo->cursory += getFont()->height + wo->textpadding;
         wo->cursorx = wo->textpadding + 1;
      }
   }

   // draw cursor
   draw_rect(wo->window_surface, 0, wo->x + wo->cursorx + offsetX, wo->y + wo->cursory + offsetY, getFont()->width, 2);
}

void windowobj_dragged(void *windowobj, int x, int y, int relX, int relY) {
   windowobj_t *wo = (windowobj_t*)windowobj;
   if(!wo->visible || (wo->parent != NULL && !((windowobj_t*)wo->parent)->visible)) return;

   if(wo->drag_func != NULL) {
      wo->drag_func(windowobj, x, y);
   }

   if(wo->type == WO_SCROLLER && wo->parent != NULL) {
      wo->y -= relY;
      if(wo->y < 14)
         wo->y = 14;
      if(wo->y + wo->height > ((windowobj_t*)wo->parent)->height - 14)
         wo->y = ((windowobj_t*)wo->parent)->height - 14 - wo->height;
      window_scroll_update(wo);
   }

   // check children
   for(int i = 0; i < wo->child_count; i++) {
      windowobj_t *child = (windowobj_t*)wo->children[i];
      if(x >= child->x && x < child->x + child->width
      && y >= child->y && y < child->y + child->height
      && child->visible) {
         // drag child
         windowobj_dragged(child, x - child->x, y - child->y, relX, relY);
         return; // stop at first child dragged
      }
   }
}

void windowobj_free(windowobj_t *wo) {
   //free text
   if(wo->text != NULL) {
      free((uint32_t)wo->text, strlen(wo->text)+1);
      wo->text = NULL;
   }
   // free children
   for(int i = 0; i < wo->child_count; i++) {
      windowobj_t *child = (windowobj_t*)wo->children[i];
      if(child != NULL) {
         windowobj_free(child);
         wo->children[i] = NULL;
      }
   }
   // free windowobj
   free((uint32_t)wo, sizeof(windowobj_t));
}