#include "ui_mgr.h"

#include "../../../lib/string.h"
#include "../stdio.h"

ui_mgr_t *ui_init(surface_t *surface, int window) {
   ui_mgr_t *ui = malloc(sizeof(ui_mgr_t));
   ui->wo_count = 0;
   ui->focused = NULL;
   ui->hovered = NULL;
   ui->surface = surface;
   ui->window = window;
   return ui;
}

int ui_add(ui_mgr_t *ui, wo_t *wo) {
   // register wo with ui_mgr
   if(ui->wo_count == MAX_WO)
      return -1;

   int index = ui->wo_count;
   ui->wos[ui->wo_count++] = wo;

   return index;
}

void ui_draw(ui_mgr_t *ui) {
   for(int i = 0; i < ui->wo_count; i++) {
      if(ui->wos[i] && ui->wos[i]->enabled && ui->wos[i]->visible && ui->wos[i]->draw_func)
         ui->wos[i]->draw_func(ui->wos[i], ui->surface, ui->window);
   }
}

void ui_click(ui_mgr_t *ui, int x, int y) {
   // check if any ui elements are clicked
   if(ui->focused)
      ui->focused = NULL;

   for(int i = 0; i < ui->wo_count; i++) {
      wo_t *wo = ui->wos[i];
      if(!(wo && wo->enabled && wo->visible))
         continue;
      if(x >= wo->x && x < wo->x + wo->width
      && y >= wo->y && y < wo->y + wo->height) {
         ui->clicked = wo;
         wo->clicked = true;
         if(wo->draw_func)
            wo->draw_func(wo, ui->surface, ui->window);
         if(wo->click_func)
            wo->click_func(wo, ui->surface, ui->window, x - wo->x, y - wo->y);
         return;
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
         wo->draw_func(wo, ui->surface, ui->window);
      if(x >= wo->x && x < wo->x + wo->width
      && y >= wo->y && y < wo->y + wo->height) {
         // call release func
         if(wo->release_func)
            wo->release_func(wo, ui->surface, ui->window, x - wo->x, y - wo->y);
         if(wo->type == WO_INPUT) {
            ui->focused = wo;
            wo->selected = true;
            if(wo->draw_func)
               wo->draw_func(wo, ui->surface, ui->window);
         }
         break;
      }
   }
   ui->clicked = NULL;
}

void ui_keypress(ui_mgr_t *ui, uint16_t c) {
   if(ui->focused) {
      wo_t *input = ui->focused;
      if(input->keypress_func)
         input->keypress_func(input, c);
      if(input->draw_func)
         input->draw_func(input, ui->surface, ui->window);
   }
}

void ui_hover(ui_mgr_t *ui, int x, int y) {
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
         // draw on hover
         if(wo != hovered && wo->draw_func)
            wo->draw_func(wo, ui->surface, ui->window);
      }
   }

   // draw on unhover
   if(hovered != ui->hovered) {
      if(hovered && hovered->draw_func)
         hovered->draw_func(hovered, ui->surface, ui->window);
   }

}

void ui_unfocus() {
   // window unfocus event

}