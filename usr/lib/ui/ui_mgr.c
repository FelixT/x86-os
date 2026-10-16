#include "ui_mgr.h"

#include "../../../lib/string.h"
#include "../stdio.h"
#include "../../prog.h"
#include "ui_menu.h"

ui_mgr_t *ui_init(surface_t *surface, int window) {
   ui_mgr_t *ui = malloc(sizeof(ui_mgr_t));
   ui->wo_count = 0;
   ui->focused = NULL;
   ui->hovered = NULL;
   ui->clicked = NULL;
   ui->surface = surface;
   ui->window = window;
   ui->default_menu = NULL;
   ui->shown_menu = NULL;
   ui->scrolled_y = 0;
   return ui;
}

int ui_add(ui_mgr_t *ui, wo_t *wo) {
   // register wo with ui_mgr

   // find free index
   for(int i = 0; i < ui->wo_count; i++) {
      if(!ui->wos[i] || !ui->wos[i]->enabled) {
         if(ui->wos[i]) {
            if(ui->hovered == ui->wos[i])
               ui->hovered = NULL;
            if(ui->focused == ui->wos[i])
               ui->focused = NULL;
            if(ui->clicked == ui->wos[i])
               ui->clicked = NULL;
            free(ui->wos[i]);
         }
         ui->wos[i] = wo;
         return i;
      }
   }

   if(ui->wo_count == MAX_WO)
      return -1;

   int index = ui->wo_count;
   ui->wos[ui->wo_count++] = wo;

   return index;
}

draw_context_t ui_get_context(ui_mgr_t *ui) {
   // returns default context
   rect_t clipRect = { .x = 0, .y = 0, .width = ui->surface->width, .height = ui->surface->height };
   return (draw_context_t) { .surface = ui->surface, .window = ui->window, .offsetX = 0, .offsetY = 0, .clipRect = clipRect };
}

void ui_draw(ui_mgr_t *ui) {
   draw_context_t context = ui_get_context(ui);
   // fixed drawn in 2nd pass
   for(int pass = 0; pass < 2; pass++) {
      bool fixed = pass == 1;
      for(int i = 0; i < ui->wo_count; i++) {
         wo_t *wo = ui->wos[i];
         if(wo && wo->fixed == fixed && wo->enabled && wo->visible && wo->draw_func)
            wo->draw_func(wo, context);
      }
   }
   if(ui->shown_menu)
      ui->shown_menu->draw_func(ui->shown_menu, context);
}

void ui_redraw_window(ui_mgr_t *ui) {
   redraw_w(ui->window);
}

void ui_redraw_wo(ui_mgr_t *ui, wo_t *wo) {
   redraw_region(wo->x, wo->y, wo->width, wo->height, ui->window);
}

void ui_redraw(ui_mgr_t *ui) {
   ui_draw(ui);
   ui_redraw_window(ui);
}

// fixed layer first (prioritised for actions)
static wo_t *ui_wo_at(ui_mgr_t *ui, int x, int y) {
   for(int pass = 0; pass < 2; pass++) {
      bool fixed = pass == 0;
      for(int i = ui->wo_count-1; i >= 0; i--) {
         wo_t *wo = ui->wos[i];
         if(!(wo && wo->fixed == fixed && wo->enabled && wo->visible))
            continue;
         if(x >= wo->x && x < wo->x + wo->width
         && y >= wo->y && y < wo->y + wo->height)
            return wo;
      }
   }
   return NULL;
}

bool hid_menu = false;

static void ui_finish_click(ui_mgr_t *ui) {
   // a click that never got its release, e.g. the click handler opened a
   // new window, which became selected and was sent the release instead.
   // finish it the way ui_release would have
   wo_t *wo = ui->clicked;
   if(!wo) return;
   wo->clicked = false;
   if(wo->focusable && !ui->focused) {
      ui->focused = wo;
      wo->selected = true;
   }
   ui->clicked = NULL;
}

bool ui_click(ui_mgr_t *ui, int x, int y) {
   draw_context_t context = ui_get_context(ui);
   if(ui->shown_menu) {
      hid_menu = true;
      ui->shown_menu->visible = false;
      bool menu_clicked = false;
      if(x >= ui->shown_menu->x && x < ui->shown_menu->x + ui->shown_menu->width
      && y >= ui->shown_menu->y && y < ui->shown_menu->y + ui->shown_menu->height) {
         ui->shown_menu->click_func(ui->shown_menu, context, x - ui->shown_menu->x, y - ui->shown_menu->y);
         menu_clicked = true;
      }
      ui->shown_menu->visible = false;
      ui->shown_menu = NULL;
      clear_w(ui->window);
      ui_redraw(ui);
      return menu_clicked;
   }
   hid_menu = false;

   ui_finish_click(ui);

   wo_t *focused = ui->focused;
   if(ui->focused) {
      ui->focused->selected = false;
      ui->focused = NULL;
   }

   wo_t *wo = ui_wo_at(ui, x, y);
   bool clicked = wo != NULL;
   if(wo) {
      ui->clicked = wo;
      wo->clicked = true;
      if(wo->click_func)
         wo->click_func(wo, context, x - wo->x, y - wo->y);
      else if(wo->draw_func)
         wo->draw_func(wo, context);

      if(wo->click_func || wo->draw_func)
         ui_redraw_wo(ui, wo);
   }

   if(focused && focused != ui->clicked) {
      if(focused->unfocus_func)
         focused->unfocus_func(focused, context);
      else if(focused->draw_func)
         focused->draw_func(focused, context);

      if(focused->unfocus_func || focused->draw_func)
         ui_redraw_wo(ui, focused);
   }

   return clicked;
}

void ui_focus(ui_mgr_t *ui, wo_t *focus) {
   // simulate click/focus to wo (doesn't draw)
   if(!focus->focusable) return;

   if(ui->focused) {
      ui->focused->selected = false;
      ui->focused = NULL;
   }
   focus->selected = true;
   ui->focused = focus;
}

void ui_release(ui_mgr_t *ui, int x, int y) {
   if(hid_menu) {
      hid_menu = false;
      return;
   }

   draw_context_t context = ui_get_context(ui);
   for(int i = 0; i < ui->wo_count; i++) {
      wo_t *wo = ui->wos[i];
      if(!(wo && wo->enabled && wo->visible) || (!wo->clicked && !wo->selected))
         continue;
      wo->clicked = false;
      if(x >= wo->x && x < wo->x + wo->width
      && y >= wo->y && y < wo->y + wo->height) {
         if(wo->focusable) {
            ui->focused = wo;
            wo->selected = true;
         }
         // call release func
         if(wo->release_func) {
            wo->release_func(wo, context, x - wo->x, y - wo->y);
            ui_redraw_wo(ui, wo);
         } else if(wo->focusable && wo->draw_func) {
            wo->draw_func(wo, context);
            ui_redraw_wo(ui, wo);
         }
            
         break;
      }
   }

   // released outside the clicked wo, so it didn't get focus. this is also
   // the case when the press went to another window: the first click on an
   // inactive window only activates it, but its release still comes here
   wo_t *clicked = ui->clicked;
   if(clicked && clicked != ui->focused && clicked->focusable
   && clicked->enabled && clicked->visible) {
      if(clicked->unfocus_func)
         clicked->unfocus_func(clicked, context);
      else if(clicked->draw_func)
         clicked->draw_func(clicked, context);

      if(clicked->unfocus_func || clicked->draw_func)
         ui_redraw_wo(ui, clicked);
   }
   ui->clicked = NULL;
}

void ui_drag(ui_mgr_t *ui, int x, int y) {
   if(!ui->clicked || !ui->clicked->drag_func) return;
   ui->clicked->drag_func(ui->clicked, ui_get_context(ui), x, y);
}

void ui_bring_to_front(ui_mgr_t *ui, wo_t *wo) {
   int index = -1;
   for(int i = 0; i < ui->wo_count; i++) {
      if(!ui->wos[i]) continue;
      if(ui->wos[i] == wo) {
         index = i;
         break;
      }
   }
   if(index == -1 || index == ui->wo_count-1) return;
   memmove(&ui->wos[index], &ui->wos[index + 1], sizeof(wo_t*) * (ui->wo_count - 1 - index));
   ui->wos[ui->wo_count - 1] = wo;
}

void ui_keypress(ui_mgr_t *ui, uint16_t c) {
   if(c == 0x80 || c == 0x81)
      return; // ignore control & shift

   if(ui->focused) {
      wo_t *wo = ui->focused;
      draw_context_t context = ui_get_context(ui);
      if(wo->keypress_func)
         wo->keypress_func(wo, c, ui->window);
      if(wo->draw_func)
         wo->draw_func(wo, context);

      if(wo->keypress_func || wo->draw_func)
         ui_redraw_wo(ui, wo);
   }
}

void ui_hover(ui_mgr_t *ui, int x, int y) {
   draw_context_t context = ui_get_context(ui);
   if(ui->shown_menu) {
      if(x >= ui->shown_menu->x && x < ui->shown_menu->x + ui->shown_menu->width
      && y >= ui->shown_menu->y && y < ui->shown_menu->y + ui->shown_menu->height) {
         ui->shown_menu->hovering = true;
         ui->shown_menu->hover_func(ui->shown_menu, context, x - ui->shown_menu->x, y - ui->shown_menu->y);
         ui_redraw_window(ui);
      } else {
         if(ui->shown_menu->hovering) {
            ui->shown_menu->hovering = false;
            ui_redraw_window(ui);
         }
      }
      return;
   }

   wo_t *hovered = ui->hovered;
   if(hovered) {
      ui->hovered->hovering = false;
      ui->hovered = NULL;
   }

   // can only hover one object at a time
   wo_t *wo = ui_wo_at(ui, x, y);
   if(wo) {
      wo->hovering = true;
      ui->hovered = wo;

      bool redraw = false;

      if(wo->hover_func) {
         wo->hover_func(wo, context, x - wo->x, y - wo->y);
         redraw = true;
      }

      if(wo != hovered) {
         if(wo->mousein_func) {
            wo->mousein_func(wo, context, x - wo->x, y - wo->y);
            redraw = true;
         } else if(wo->draw_func) {
            wo->draw_func(wo, context);
            redraw = true;
         }
      }

      if(redraw) ui_redraw_wo(ui, wo);
   }

   // draw on unhover
   if(hovered != ui->hovered && hovered && hovered->enabled && hovered->visible) {
      if(hovered->unhover_func) {
         hovered->unhover_func(hovered, context);
         ui_redraw_wo(ui, hovered);
      } else if(hovered->draw_func) {
         hovered->draw_func(hovered, context);
         ui_redraw_wo(ui, hovered);
      }
   }

}

void ui_rightclick(ui_mgr_t *ui, int x, int y) {
   wo_t *menu = ui->default_menu;
   if(!menu) return;
   draw_context_t context = ui_get_context(ui);
   if(menu->visible) {
      menu->visible = false;
      clear_w(ui->window);
      ui_redraw(ui);
      return;
   }
   menu->visible = true;
   menu->x = x;
   menu->y = y;
   if(menu->x + menu->width > ui->surface->width)
      menu->x = ui->surface->width - menu->width;
   if(menu->y + menu->height > ui->surface->height)
      menu->y = ui->surface->height - menu->height;
   menu_t *menu_data = menu->data;
   menu_data->selected_index = -1;
   menu_data->hover_index = -1;
   if(menu->draw_func)
      menu->draw_func(menu, context);
   ui->shown_menu = menu;
}

bool ui_remove(ui_mgr_t *ui, wo_t *wo) {
   if(ui->hovered == wo)
      ui->hovered = NULL;
   if(ui->clicked == wo)
      ui->clicked = NULL;
   if(ui->focused == wo)
      ui->focused = NULL;
   for(int i = 0; i < ui->wo_count; i++) {
      if(ui->wos[i] != wo) continue;
      ui->wos[i] = NULL;
      return true;
   }
   return false;
}

void ui_unfocus() {
   // window unfocus event

}

void ui_scroll_buffer(ui_mgr_t *ui, int deltaY) {
   uint16_t *fb = (uint16_t*)ui->surface->buffer;
   int bg = get_window_setting(W_SETTING_BGCOLOUR, ui->window);

   int absY = -deltaY;
   if(deltaY > 0 && deltaY < ui->surface->height) {
      // scroll down

      // copy
      memmove(fb, fb + deltaY*ui->surface->width, (ui->surface->height-deltaY)*ui->surface->width*sizeof(uint16_t));
      // clear bottom
      memset16(fb + (ui->surface->height - deltaY)*ui->surface->width, bg, deltaY*ui->surface->width);
   } else if(deltaY < 0 && absY < ui->surface->height - 1) {
      // scroll up
      memmove(fb + absY*ui->surface->width, fb, (ui->surface->height-absY)*ui->surface->width*sizeof(uint16_t));

      // clear top
      memset16(fb, bg, absY*ui->surface->width);
   }
}

void ui_scroll_buffer_c(ui_mgr_t *ui, int deltaY, draw_context_t context) {
   uint16_t *fb = (uint16_t*)ui->surface->buffer;
   int bg = get_window_setting(W_SETTING_BGCOLOUR, ui->window);
   int absY = -deltaY;
    
   int clip_x = context.clipRect.x;
   int clip_y = context.clipRect.y;
   int clip_w = context.clipRect.width;
   int clip_h = context.clipRect.height;
   int buffer_w = ui->surface->width;
    
   if(deltaY > 0 && deltaY < clip_h) {
      // scroll down
      for(int row = clip_h - 1; row >= deltaY; row--) {
         uint16_t *dst = fb + (clip_y + row) * buffer_w + clip_x;
         uint16_t *src = fb + (clip_y + row - deltaY) * buffer_w + clip_x;
         memmove(dst, src, clip_w * sizeof(uint16_t));
      }
      // clear top
      for(int row = 0; row < deltaY; row++) {
         uint16_t *dst = fb + (clip_y + row) * buffer_w + clip_x;
         memset16(dst, bg, clip_w);
      }
   } else if(deltaY < 0 && absY < clip_h) {
      // scroll up
      for(int row = 0; row < clip_h - absY; row++) {
         uint16_t *dst = fb + (clip_y + row) * buffer_w + clip_x;
         uint16_t *src = fb + (clip_y + row + absY) * buffer_w + clip_x;
         memmove(dst, src, clip_w * sizeof(uint16_t));
      }
      // clear bottom
      for(int row = clip_h - absY; row < clip_h; row++) {
         uint16_t *dst = fb + (clip_y + row) * buffer_w + clip_x;
         memset16(dst, bg, clip_w);
      }
   }
}

// default ui scroll behaviour
void ui_scroll(ui_mgr_t *ui, int deltaY, int offsetY) {
   ui->scrolled_y = offsetY;
   
   uint16_t *fb = (uint16_t*)ui->surface->buffer;
   int bg = get_window_setting(W_SETTING_BGCOLOUR, ui->window);

   for(int i = 0; i < ui->wo_count; i++) {
      wo_t *wo = ui->wos[i];
      if(!wo) continue;
      if(wo->fixed) {
         memset16(fb + wo->y*ui->surface->width, bg, wo->height*ui->surface->width);
         continue;
      }
      wo->y -= deltaY;
   }

   ui_scroll_buffer(ui, deltaY);
}
