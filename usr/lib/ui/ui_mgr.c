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
         if(ui->wos[i])
            free(ui->wos[i], sizeof(wo_t));
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

void ui_draw(ui_mgr_t *ui) {
   for(int i = 0; i < ui->wo_count; i++) {
      if(ui->wos[i] && ui->wos[i]->enabled && ui->wos[i]->visible && ui->wos[i]->draw_func)
         ui->wos[i]->draw_func(ui->wos[i], ui->surface, ui->window, 0, 0);
   }
}

void ui_redraw(ui_mgr_t *ui) {
   ui_draw(ui);
   redraw_w(ui->window);
}

void ui_click(ui_mgr_t *ui, int x, int y) {
   if(ui->default_menu && ui->default_menu->visible) {
      ui->default_menu->visible = false;
      if(x >= ui->default_menu->x && x < ui->default_menu->x + ui->default_menu->width
      && y >= ui->default_menu->y && y < ui->default_menu->y + ui->default_menu->height) {
         clear_w(ui->window);
         ui_draw(ui);
         redraw_w(ui->window);
         ui->default_menu->click_func(ui->default_menu, ui->surface, ui->window, x - ui->default_menu->x, y - ui->default_menu->y, 0, 0);
      }
      clear_w(ui->window);
      ui_draw(ui);
      return;
   }

   // check if any ui elements are clicked
   if(ui->focused) {
      ui->focused->selected = false;
      if(ui->focused->unfocus_func)
         ui->focused->unfocus_func(ui->focused, ui->surface, ui->window, 0, 0);
      else if(ui->focused->draw_func)
         ui->focused->draw_func(ui->focused, ui->surface, ui->window, 0, 0);
      ui->focused = NULL;
   }

   for(int i = 0; i < ui->wo_count; i++) {
      wo_t *wo = ui->wos[i];
      if(!(wo && wo->enabled && wo->visible))
         continue;
      if(x >= wo->x && x < wo->x + wo->width
      && y >= wo->y && y < wo->y + wo->height) {
         ui->clicked = wo;
         wo->clicked = true;
         if(wo->click_func)
            wo->click_func(wo, ui->surface, ui->window, x - wo->x, y - wo->y, 0, 0);
         else if(wo->draw_func)
            wo->draw_func(wo, ui->surface, ui->window, 0, 0);
         break;
      }
   }
}

void ui_release(ui_mgr_t *ui, int x, int y) {
   for(int i = 0; i < ui->wo_count; i++) {
      wo_t *wo = ui->wos[i];
      if(!(wo && wo->enabled && wo->visible) || (!wo->clicked && !wo->selected))
         continue;
      wo->clicked = false;
      wo->selected = false;
      if(wo->draw_func)
         wo->draw_func(wo, ui->surface, ui->window, 0, 0);
      if(x >= wo->x && x < wo->x + wo->width
      && y >= wo->y && y < wo->y + wo->height) {
         // call release func
         if(wo->release_func)
            wo->release_func(wo, ui->surface, ui->window, x - wo->x, y - wo->y, 0, 0);
         if(wo->type == WO_INPUT || wo->type == WO_MENU || wo->type == WO_CANVAS || wo->type == WO_GRID || wo->type == WO_GROUPBOX) {
            ui->focused = wo;
            wo->selected = true;
            if(!wo->release_func && wo->draw_func)
               wo->draw_func(wo, ui->surface, ui->window, 0, 0);
         }
         break;
      }
   }
   ui->clicked = NULL;
}

void ui_keypress(ui_mgr_t *ui, uint16_t c) {
   if(ui->focused) {
      wo_t *wo = ui->focused;
      if(wo->keypress_func)
         wo->keypress_func(wo, c, ui->window);
      if(wo->draw_func)
         wo->draw_func(wo, ui->surface, ui->window, 0, 0);
   }
}

void ui_hover(ui_mgr_t *ui, int x, int y) {
   if(ui->default_menu && ui->default_menu->visible) {
      if(x >= ui->default_menu->x && x < ui->default_menu->x + ui->default_menu->width
      && y >= ui->default_menu->y && y < ui->default_menu->y + ui->default_menu->height) {
         ui->default_menu->hovering = true;
         ui->default_menu->hover_func(ui->default_menu, ui->surface, ui->window, x - ui->default_menu->x, y - ui->default_menu->y, 0, 0);
      } else {
         ui->default_menu->hovering = false;
         ui->default_menu->draw_func(ui->default_menu, ui->surface, ui->window, 0, 0);
      }
      return;
   }

   wo_t *hovered = ui->hovered;
   if(hovered) {
      ui->hovered->hovering = false;
      ui->hovered = NULL;
   }

   for(int i = 0; i < ui->wo_count; i++) {
      wo_t *wo = ui->wos[i];
      if(!(wo && wo->enabled && wo->visible))
         continue;
      if(x >= wo->x && x < wo->x + wo->width
      && y >= wo->y && y < wo->y + wo->height) {
         wo->hovering = true;
         ui->hovered = wo;

         if(wo->hover_func)
            wo->hover_func(wo, ui->surface, ui->window, x - wo->x, y - wo->y, 0, 0);
         
         if(wo != hovered) {
            if(wo->mousein_func)
               wo->mousein_func(wo, ui->surface, ui->window, x - wo->x, y - wo->y, 0, 0);
            else if(wo->draw_func)
               wo->draw_func(wo, ui->surface, ui->window, 0, 0);
         }
      }
   }

   // draw on unhover
   if(hovered != ui->hovered && hovered && hovered->enabled && hovered->visible) {
      if(hovered->unhover_func) {
         hovered->unhover_func(hovered, ui->surface, ui->window, 0, 0);
      } else if(hovered->draw_func) {
         hovered->draw_func(hovered, ui->surface, ui->window, 0, 0);
      }
   }

}

void ui_rightclick(ui_mgr_t *ui, int x, int y) {
   wo_t *menu = ui->default_menu;
   if(menu) {
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
         menu->draw_func(menu, ui->surface, ui->window, 0, 0);
   }
}

void ui_unfocus() {
   // window unfocus event

}

void ui_scroll(ui_mgr_t *ui, int deltaY, int offsetY) {
   ui->scrolled_y = offsetY;
   for(int i = 0; i < ui->wo_count; i++) {
      wo_t *wo = ui->wos[i];
      if(wo->fixed) continue;
      wo->y -= deltaY;
   }
   clear_w(ui->window);
   ui_draw(ui);
}
