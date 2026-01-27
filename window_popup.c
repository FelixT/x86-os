#include <stddef.h>
#include "window_popup.h"
#include "windowmgr.h"
#include "window.h"
#include "windowobj.h"
#include "lib/string.h"
#include "memory.h"
#include "interrupts.h"

// window popup - subwindows

void window_popup_init(gui_window_t *window, gui_window_t *parent) {
   window->keypress_func = NULL;
   window->draw_func = NULL;
   if(parent != NULL)
      strcpy(window->title, parent->title); // take name from parent
}

// define default popups - common mini-progs

void window_popup_dialog_close(void *windowobj, void *regs) {
   (void)windowobj;
   gui_window_t *window = getSelectedWindow();
   window_popup_dialog_t *dialog = (window_popup_dialog_t*)window->state;
   int index = get_window_index_from_pointer(window);
   int parent_index = get_window_index_from_pointer(dialog->parent);

   // launch callback if exists
   if(dialog->callback_func == NULL) {
      // self destruct
      debug_printf("Closing window %i\n", index);
      window_close(NULL, index);
      return;
   }

   if(get_task_from_window(getSelectedWindowIndex()) == -1) {
      // call as kernel
      dialog->callback_func(regs);
      if(getSelectedWindow()) {
         getSelectedWindow()->needs_redraw = true;
         window_draw(getSelectedWindow());
      }
   }
   
   setSelectedWindowIndex(parent_index);

   // self destruct
   debug_printf("Closing window %i\n", index);
   window_close(NULL, index);
}

window_popup_dialog_t *window_popup_dialog(gui_window_t *window, gui_window_t *parent, char *text) {
   if(parent != NULL)
      parent->children[parent->child_count++] = window;
   
   int height = 95;
   
   window_resize(NULL, window, 260, height);

   window_popup_init(window, parent);
   // add default window objects

   strcpy(window->title, "Msg");
   if(parent != NULL) {
      window->x = parent->x + 50;
      window->y = parent->y + 50;
   }
   if(window->y + window->height > (int)gui_get_height())
      window->y = gui_get_height() - window->height;

   window_popup_dialog_t *dialog = malloc(sizeof(window_popup_dialog_t));
   dialog->parent = parent;
   dialog->callback_func = NULL;
   window->state = (void*)dialog;
   window->state_size = sizeof(window_popup_dialog_t);
   window->resizable = false;

   // dialog message
   int y = 5;
   windowobj_t *wo_msg = window_create_text(window, 15, y, text);
   wo_msg->width = 230;
   wo_msg->height = 30;
   wo_msg->disabled = true;
   wo_msg->bordered = false;
   wo_msg->textvalign = true;
   wo_msg->texthalign = true;
   y += 40;

   // ok button
   int x = 105;
   dialog->wo_okbtn = window_create_button(window, x, y, "Ok", &window_popup_dialog_close);

   window_clearbuffer(window, window->bgcolour);

   return dialog;
}

void window_popup_colourpicker_click(int x, int y) {
   if(x >= 5 && x < 261 && y >= 30 && y < 306) {
      // clicked in colour picker area
      x -= 5; // offset
      y -= 30; // offset
      if(x < 0 || x >= 256 || y < 0 || y >= 256) return; // out of bounds

      gui_window_t *window = getSelectedWindow();

      // calculate colour
      uint16_t colour = window->framebuffer[(y+30)*window->width + (x+5)];
      char hex[7];
      sprintf(hex, "%h", colour);
      windowobj_t *wo_col = window->window_objects[0];
      strcpy(wo_col->text, hex);

      // draw colour square
      for(int x = 0; x < 50; x++) {
         for(int y = 0; y < 50; y++) {
            window->framebuffer[(y + 30) * window->width + (x + 265)] = colour;
         }
      }
   }
}

void window_popup_colourpicker_return(void *windowobj, void *regs, int x, int y) {
   (void)windowobj;
   (void)x;
   (void)y;
   //windowobj_t *wo = (windowobj_t*)windowobj;
   gui_window_t *window = getSelectedWindow();
   window_popup_colourpicker_t *cp = (window_popup_colourpicker_t*)window->state;
   uint16_t colour = (uint16_t)hextouint(window->window_objects[0]->text);
   void (*callback)(uint16_t) = cp->callback_func;

   setSelectedWindowIndex(get_window_index_from_pointer(cp->parent));

   // self destruct
   window_close(NULL, get_window_index_from_pointer(window));
   
   if(getSelectedWindow() == NULL) return;

   // call callback function
   if(callback != NULL) {

      if(get_task_from_window(getSelectedWindowIndex()) == -1) {
         // call as kernel
         callback(colour);
      } else {
         if(!gui_interrupt_switchtask(regs)) return;
         // calling function as task
         uint32_t *args = malloc(sizeof(uint32_t) * 1);
         args[0] = (uint32_t)colour;
         task_call_subroutine(regs, "colourpickerreturn", (uint32_t)callback, args, 1);
      }

      getSelectedWindow()->needs_redraw = true;
      window_draw(getSelectedWindow());
   }

}

// init
window_popup_colourpicker_t *window_popup_colourpicker(gui_window_t *window, gui_window_t *parent, void *callback, uint16_t colour) {
   parent->children[parent->child_count++] = window;

   window_resize(NULL, window, 320, 340);
   window_popup_init(window, parent);
   strcpy(window->title, "Colour Picker");

   window->x = parent->x + 50;
   window->y = parent->y + 50;
   if(window->y + window->height > (int)gui_get_height())
      window->y = gui_get_height() - window->height;

   window->click_func = &window_popup_colourpicker_click;
   window->drag_func = &window_popup_colourpicker_click;

   window->resizable = false;
   window->state_size = sizeof(window_popup_colourpicker_t);
   window->state = malloc(window->state_size);
   window_popup_colourpicker_t *cp = (window_popup_colourpicker_t*)window->state;
   cp->callback_func = callback;
   cp->parent = parent;

   int y = 5;
   windowobj_t *wo_col = malloc(sizeof(windowobj_t));
   windowobj_init(wo_col, &window->surface);
   wo_col->type = WO_TEXT;
   wo_col->x = 5;
   wo_col->y = y;
   wo_col->width = 190;
   wo_col->height = 16;
   wo_col->text = malloc(10);
   uinttohexstr(colour, wo_col->text);
   wo_col->texthalign = false;
   wo_col->disabled = true;
   window->window_objects[window->window_object_count++] = wo_col;

   // ok button
   windowobj_t *wo_okbtn = malloc(sizeof(windowobj_t));
   windowobj_init(wo_okbtn, &window->surface);
   wo_okbtn->type = WO_BUTTON;
   wo_okbtn->x = 205;
   wo_okbtn->y = y;
   wo_okbtn->width = 110;
   wo_okbtn->height = 16;
   wo_okbtn->text = malloc(strlen("OK") + 1);
   strcpy(wo_okbtn->text, "OK");
   wo_okbtn->release_func = &window_popup_colourpicker_return;
   window->window_objects[window->window_object_count++] = wo_okbtn;

   // draw colour square border
   draw_unfilledrect(&window->surface, rgb16(0, 0, 0), 264, 29, 52, 52); // border
   // draw colour square
   for(int x = 0; x < 50; x++) {
      for(int y = 0; y < 50; y++) {
         window->framebuffer[(y + 30) * window->width + (x + 265)] = colour;
      }
   }
   // draw the colour picker at (5, 50)
   draw_unfilledrect(&window->surface, rgb16(0, 0, 0), 4, 29, 258, 258); // border
   int xoffset = 5;
   int yoffset = 30;
   for(int x = 0; x < 256; x++) {
      for(int y = 0; y < 256; y++) {
         uint16_t colour = 0;
         
         if(x < 128 && y < 128) {
            // top left - red gradient
            uint16_t red = (x * 31) / 127;      // scale to 5-bit (0-31)
            uint16_t green = (y * 63) / 127;    // scale to 6-bit (0-63)
            colour = (red << 11) | (green << 5) | 0;  // blue = 0
               
         } else if(x >= 128 && y < 128) {
            // top right - green gradient
            uint16_t green = ((x - 128) * 63) / 127;  // scale (x-128) to 6-bit
            uint16_t blue = (y * 31) / 127;           // scale y to 5-bit
            colour = (0 << 11) | (green << 5) | blue; // red = 0
               
         } else if(x < 128 && y >= 128) {
            // bottom left - blue gradient
            uint16_t red = (x * 31) / 127;            // scale x to 5-bit
            uint16_t blue = ((y - 128) * 31) / 127;   // scale (y-128) to 5-bit
            colour = (red << 11) | (0 << 5) | blue;   // green = 0
               
         } else {
            // bottom right - white to black gradient
            uint16_t intensity = ((x - 128) + (y - 128)) / 2;  // 0-127
            uint16_t red = (intensity * 31) / 127;
            uint16_t green = (intensity * 63) / 127;
            uint16_t blue = (intensity * 31) / 127;
            colour = (red << 11) | (green << 5) | blue;
         }
         
         window->framebuffer[(y+yoffset)*window->width + x+xoffset] = colour;
      }
   }

   return cp;

}