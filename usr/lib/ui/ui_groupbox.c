#include "ui_groupbox.h"

#include "ui_canvas.h"
#include "../draw.h"
#include "../../prog.h"
#include "../../../lib/string.h"
#include "wo.h"

// win32 style groupbox/fieldset
// made up of a label, border & canvas wo

draw_context_t groupbox_get_context(wo_t *groupbox, draw_context_t context) {
   context.offsetX += groupbox->x;
   context.offsetY += groupbox->y;
   context.clipRect.x = context.offsetX;
   context.clipRect.y = context.offsetY;
   context.clipRect.width -= groupbox->x;
   context.clipRect.height -= groupbox->y;
   if(context.clipRect.width > groupbox->width)
      context.clipRect.width = groupbox->width;
   if(context.clipRect.height > groupbox->height)
      context.clipRect.height = groupbox->height;
   return context;
}

void click_groupbox(wo_t *groupbox, draw_context_t context, int x, int y) {
   if(groupbox == NULL || groupbox->data == NULL) return;
   groupbox_t *groupbox_data = groupbox->data;

   wo_t *canvas = groupbox_data->canvas;
   context = groupbox_get_context(groupbox, context);

   if(x >= canvas->x && x < canvas->x + canvas->width
   && y >= canvas->y && y < canvas->y + canvas->height) {
      canvas->click_func(canvas, context, x - canvas->x, y - canvas->y);
   }
}

void release_groupbox(wo_t *groupbox, draw_context_t context, int x, int y) {
   if(groupbox == NULL || groupbox->data == NULL) return;
   groupbox_t *groupbox_data = groupbox->data;

   wo_t *canvas = groupbox_data->canvas;
   context = groupbox_get_context(groupbox, context);

   if(x >= canvas->x && x < canvas->x + canvas->width
   && y >= canvas->y && y < canvas->y + canvas->height) {
      canvas->release_func(canvas, context, x - canvas->x, y - canvas->y);
   }
}

void unfocus_groupbox(wo_t *groupbox, draw_context_t context) {
   if(groupbox == NULL || groupbox->data == NULL) return;
   groupbox_t *groupbox_data = groupbox->data;

   wo_t *canvas = groupbox_data->canvas;
   context = groupbox_get_context(groupbox, context);
   canvas->unfocus_func(canvas, context);
}

void hover_groupbox(wo_t *groupbox, draw_context_t context, int x, int y) {
   if(groupbox == NULL || groupbox->data == NULL) return;
   groupbox_t *groupbox_data = groupbox->data;

   wo_t *canvas = groupbox_data->canvas;
   context = groupbox_get_context(groupbox, context);

   if(x >= canvas->x && x < canvas->x + canvas->width
   && y >= canvas->y && y < canvas->y + canvas->height) {
      canvas->hovering = true;
      canvas->hover_func(canvas, context, x - canvas->x, y - canvas->y);
   } else {
      if(canvas->hovering) {
         canvas->unhover_func(canvas, context);
         canvas->hovering = false;
      }
   }
}

void mousein_groupbox() {
   // do nothing
}

void keypress_groupbox(wo_t *groupbox, uint16_t c, int window) {
   if(groupbox == NULL || groupbox->data == NULL) return;
   groupbox_t *groupbox_data = groupbox->data;

   wo_t *canvas = groupbox_data->canvas;
   canvas->keypress_func(canvas, c, window);
}

void unhover_groupbox(wo_t *groupbox, draw_context_t context) {
   if(groupbox == NULL || groupbox->data == NULL) return;
   groupbox_t *groupbox_data = groupbox->data;

   wo_t *canvas = groupbox_data->canvas;
   context = groupbox_get_context(groupbox, context);
   canvas->unhover_func(canvas, context);
}

void draw_groupbox(wo_t *groupbox, draw_context_t context) {
   if(groupbox == NULL || groupbox->data == NULL) return;
   groupbox_t *groupbox_data = groupbox->data;

   write_strat_w(groupbox_data->label, groupbox->x + 5, groupbox->y, groupbox_data->colour_label, context.window);

   // draw border
   font_info_t fontinfo = get_font_info();
   int margin_top = fontinfo.height + 2;
   int font_width = strlen(groupbox_data->label)*(fontinfo.width+fontinfo.padding);
   draw_line(&context, groupbox_data->colour_border, groupbox->x, groupbox->y + margin_top/2, false, 4); // top
   draw_line(&context, groupbox_data->colour_border, groupbox->x + font_width + 5, groupbox->y + margin_top/2, false, groupbox->width - font_width - 5); // top
   draw_line(&context, groupbox_data->colour_border, groupbox->x, groupbox->y + groupbox->height - 1, false, groupbox->width); // bottom
   draw_line(&context, groupbox_data->colour_border, groupbox->x, groupbox->y + margin_top/2, true, groupbox->height - margin_top/2); // left
   draw_line(&context, groupbox_data->colour_border, groupbox->x + groupbox->width - 1, groupbox->y + margin_top/2, true, groupbox->height - margin_top/2); // right

   wo_t *canvas = groupbox_data->canvas;
   context = groupbox_get_context(groupbox, context);
   canvas->draw_func(canvas, context);
}

wo_t *create_groupbox(int x, int y, int width, int height, char *label) {
   int margin = 1;
   int margin_top = get_font_info().height + 2;

   wo_t *groupbox = create_wo(x, y, width, height);
   groupbox_t *groupbox_data = malloc(sizeof(groupbox_t));
   strncpy(groupbox_data->label, label, sizeof(groupbox_data->label)-1);
   groupbox_data->colour_label = 0;
   groupbox_data->colour_border = rgb16(200, 200, 200);
   groupbox_data->canvas = create_canvas(margin, margin_top, width - margin*2, height - margin_top - margin);
   canvas_t *canvas_data = groupbox_data->canvas->data;
   canvas_data->bordered = false;
   canvas_data->filled = false;

   groupbox->data = groupbox_data;
   groupbox->draw_func = &draw_groupbox;
   groupbox->click_func = &click_groupbox;
   groupbox->release_func = &release_groupbox;
   groupbox->hover_func = &hover_groupbox;
   groupbox->unfocus_func = &unfocus_groupbox;
   groupbox->keypress_func = &keypress_groupbox;
   groupbox->unhover_func = &unhover_groupbox;
   groupbox->mousein_func = (void*)&mousein_groupbox;
   groupbox->type = WO_GROUPBOX;
   groupbox->focusable = true;

   return groupbox;
}

void groupbox_add(wo_t *groupbox, wo_t *child) {
   if(groupbox == NULL || groupbox->data == NULL) return;
   wo_t *groupbox_canvas = ((groupbox_t*)groupbox->data)->canvas;
   canvas_add(groupbox_canvas, child);
}

void groupbox_resize(wo_t *groupbox, int width, int height) {
   if(groupbox == NULL || groupbox->data == NULL) return;
   groupbox_t *groupbox_data = groupbox->data;

   groupbox->width = width;
   groupbox->height = height;

   wo_t *canvas = groupbox_data->canvas;
   canvas->width = width - 2;
   canvas->height = height - (get_font_info().height + 2) - 1;
}

wo_t *groupbox_get_canvas(wo_t *groupbox) {
   groupbox_t *data = groupbox->data;
   return data->canvas;
}