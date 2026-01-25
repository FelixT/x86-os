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
   ui->surface = surface;
   ui->window = window;
   ui->default_menu = NULL;
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
            free(ui->wos[i], sizeof(wo_t));
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
   for(int i = 0; i < ui->wo_count; i++) {
      if(ui->wos[i] && ui->wos[i]->enabled && ui->wos[i]->visible && ui->wos[i]->draw_func)
         ui->wos[i]->draw_func(ui->wos[i], context);
   }
}

void ui_redraw(ui_mgr_t *ui) {
   ui_draw(ui);
   redraw_w(ui->window);
}

bool ui_click(ui_mgr_t *ui, int x, int y) {
   draw_context_t context = ui_get_context(ui);
   if(ui->default_menu && ui->default_menu->visible) {
      ui->default_menu->visible = false;
      if(x >= ui->default_menu->x && x < ui->default_menu->x + ui->default_menu->width
      && y >= ui->default_menu->y && y < ui->default_menu->y + ui->default_menu->height) {
         clear_w(ui->window);
         ui_draw(ui);
         redraw_w(ui->window);
         ui->default_menu->click_func(ui->default_menu, context, x - ui->default_menu->x, y - ui->default_menu->y);
         return true;
      }
      clear_w(ui->window);
      ui_draw(ui);
      return true;
   }

   // check if any ui elements are clicked
   if(ui->focused) {
      ui->focused->selected = false;
      if(ui->focused->unfocus_func)
         ui->focused->unfocus_func(ui->focused, context);
      else if(ui->focused->draw_func)
         ui->focused->draw_func(ui->focused, context);
      ui->focused = NULL;
   }

   bool clicked = false;
   for(int i = ui->wo_count-1; i >= 0; i--) {
      wo_t *wo = ui->wos[i];
      if(!(wo && wo->enabled && wo->visible))
         continue;
      if(x >= wo->x && x < wo->x + wo->width
      && y >= wo->y && y < wo->y + wo->height) {
         clicked = true;
         ui->clicked = wo;
         wo->clicked = true;
         if(wo->click_func)
            wo->click_func(wo, context, x - wo->x, y - wo->y);
         else if(wo->draw_func)
            wo->draw_func(wo, context);
         break;
      }
   }
   return clicked;
}

void ui_release(ui_mgr_t *ui, int x, int y) {
   draw_context_t context = ui_get_context(ui);
   for(int i = 0; i < ui->wo_count; i++) {
      wo_t *wo = ui->wos[i];
      if(!(wo && wo->enabled && wo->visible) || (!wo->clicked && !wo->selected))
         continue;
      wo->clicked = false;
      if(x >= wo->x && x < wo->x + wo->width
      && y >= wo->y && y < wo->y + wo->height) {
         // call release func
         if(wo->release_func)
            wo->release_func(wo, context, x - wo->x, y - wo->y);
         if(wo->focusable) {
            ui->focused = wo;
            wo->selected = true;
            if(!wo->release_func && wo->draw_func)
               wo->draw_func(wo, context);
         }
         break;
      }
   }
   ui->clicked = NULL;
}

void ui_keypress(ui_mgr_t *ui, uint16_t c) {
   if(ui->focused) {
      wo_t *wo = ui->focused;
      draw_context_t context = ui_get_context(ui);
      if(wo->keypress_func)
         wo->keypress_func(wo, c, ui->window);
      if(wo->draw_func)
         wo->draw_func(wo, context);
   }
}

void ui_hover(ui_mgr_t *ui, int x, int y) {
   draw_context_t context = ui_get_context(ui);
   if(ui->default_menu && ui->default_menu->visible) {
      if(x >= ui->default_menu->x && x < ui->default_menu->x + ui->default_menu->width
      && y >= ui->default_menu->y && y < ui->default_menu->y + ui->default_menu->height) {
         ui->default_menu->hovering = true;
         ui->default_menu->hover_func(ui->default_menu, context, x - ui->default_menu->x, y - ui->default_menu->y);
      } else {
         ui->default_menu->hovering = false;
         ui->default_menu->draw_func(ui->default_menu, context);
      }
      return;
   }

   wo_t *hovered = ui->hovered;
   if(hovered) {
      ui->hovered->hovering = false;
      ui->hovered = NULL;
   }

   for(int i = ui->wo_count-1; i >= 0; i--) {
      wo_t *wo = ui->wos[i];
      if(!(wo && wo->enabled && wo->visible))
         continue;
      if(x >= wo->x && x < wo->x + wo->width
      && y >= wo->y && y < wo->y + wo->height) {
         wo->hovering = true;
         ui->hovered = wo;

         if(wo->hover_func)
            wo->hover_func(wo, context, x - wo->x, y - wo->y);
         
         if(wo != hovered) {
            if(wo->mousein_func)
               wo->mousein_func(wo, context, x - wo->x, y - wo->y);
            else if(wo->draw_func)
               wo->draw_func(wo, context);
         }
         break; // can only hover one object at a time
      }
   }

   // draw on unhover
   if(hovered != ui->hovered && hovered && hovered->enabled && hovered->visible) {
      if(hovered->unhover_func) {
         hovered->unhover_func(hovered, context);
      } else if(hovered->draw_func) {
         hovered->draw_func(hovered, context);
      }
   }

}

void ui_rightclick(ui_mgr_t *ui, int x, int y) {
   wo_t *menu = ui->default_menu;
   if(menu) {
      draw_context_t context = ui_get_context(ui);
      if(menu->visible) {
         menu->visible = false;
         clear_w(ui->window);
         ui_draw(ui);
      }
      menu->visible = true;
      menu->x = x;
      menu->y = y;
      menu_t *menu_data = menu->data;
      menu_data->selected_index = -1;
      menu_data->hover_index = -1;
      if(menu->draw_func)
         menu->draw_func(menu, context);
   }
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
      if(wo->fixed) {
         memset16(fb + wo->y*ui->surface->width, bg, wo->height*ui->surface->width);
         continue;
      }
      wo->y -= deltaY;
   }

   ui_scroll_buffer(ui, deltaY);
}
