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

void window_popup_dialog_state_free(void *windowp, void *regs) {
   gui_window_t *window = (gui_window_t*)windowp;
   window_popup_dialog_t *dialog = (window_popup_dialog_t*)window->state;
   if(dialog == NULL) return;
   if(!dialog->answered && dialog->dismiss_func != NULL)
      dialog->dismiss_func(dialog, regs); // if window is closed
   free((uint32_t)dialog, window->state_size);
   window->state = NULL;
}

void window_popup_dialog_close(void *windowobj, int x, int y) {
   (void)windowobj;
   (void)x;
   (void)y;
   gui_window_t *window = getSelectedWindow();
   window_popup_dialog_t *dialog = (window_popup_dialog_t*)window->state;
   int index = get_window_index_from_pointer(window);
   int parent_index = get_window_index_from_pointer(dialog->parent);

   if(dialog->callback_func == NULL) {
      // self destruct
      debug_printf("Closing window %i\n", index);
      window_close(index, false);
      gui_redrawall();
      return;
   }

   if(get_task_from_window(getSelectedWindowIndex()) == -1) {
      // call as kernel
      if(dialog->callback_func) {
         dialog->answered = true; // explicit resolution - suppress dismiss default
         dialog->callback_func(dialog);
      }
   }
   setSelectedWindowIndex(parent_index);

   // self destruct
   debug_printf("Closing window %i\n", index);
   window_close(index, false);
   gui_redrawall();
}

window_popup_dialog_t *window_popup_dialog(gui_window_t *window, gui_window_t *parent, char *text) {
   if(parent != NULL) {
      parent->children[parent->child_count++] = window;
      window->parent = parent;
   }
   
   int height = 95;
   
   window_resize(NULL, window, 260, height, false);

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
   dialog->dismiss_func = NULL;
   dialog->answered = false;
   dialog->process_uid = 0;
   dialog->task_id = -1;
   window->state = (void*)dialog;
   window->state_size = sizeof(window_popup_dialog_t);
   window->state_free = &window_popup_dialog_state_free;
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
      gui_draw();
   }
}

void window_popup_colourpicker_return(void *windowobj, int x, int y) {
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
   window_close(get_window_index_from_pointer(window), false);
   gui_redrawall();
   
   if(getSelectedWindow() == NULL) return;

   // call callback function
   if(callback != NULL) {

      int taskIndex = get_task_from_window(getSelectedWindowIndex());
      if(taskIndex == -1) {
         // call as kernel
         callback(colour);
      }

      getSelectedWindow()->needs_redraw = true;
      window_draw(getSelectedWindow());
   }

}

// init
window_popup_colourpicker_t *window_popup_colourpicker(gui_window_t *window, gui_window_t *parent, void (*callback)(uint16_t colour), uint16_t colour) {
   parent->children[parent->child_count++] = window;
   window->parent = parent;

   window_resize(NULL, window, 320, 340, false);
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

// end task popup dialog
// uses window_popup_dialog_t

void endtask_callback(void *dialog) {
   // callback for close window dialog
   window_popup_dialog_t *d = (window_popup_dialog_t*)dialog;
   task_state_t *task = &gettasks()[d->task_id];
   if(!task->paused || !task->enabled || !task->process || task->process->uid != d->process_uid) {
      debug_printf("Task %i no longer paused\n", task->task_id);
      return;
   }
   int t = task->task_id;
   int w = get_task_window(t);
   if(end_task(t, NULL)) {
      window_close(w, false);
      wm_redrawall();
   }
}

// ends task if dialog is closed
void endtask_dismiss(void *dialog, void *regs) {
   window_popup_dialog_t *d = (window_popup_dialog_t*)dialog;
   task_state_t *task = &gettasks()[d->task_id];
   if(!task->paused || !task->enabled || !task->process || task->process->uid != d->process_uid) {
      debug_printf("Task %i no longer paused\n", task->task_id);
      return;
   }
   int w = get_task_window(d->task_id);
   end_task(d->task_id, regs);
   if(w < 0) return;
   wm_event(WINDOW_CLOSE, w, 0, 0);
}

void endtask_debug(void *windowobj, int x, int y) {
   (void)windowobj;
   (void)x;
   (void)y;
   window_popup_dialog_t *dialog = getSelectedWindow()->state;
   dialog->answered = true;
   task_state_t *task = &gettasks()[dialog->task_id];
   if(!task->paused || !task->enabled || !task->process || task->process->uid != dialog->process_uid) {
      debug_printf("Task %i no longer paused\n", task->task_id);
      return;
   }
   uint32_t eip = task->registers.eip;
   char *exe_path = task->process->exe_path;
   char addrbuffer[10];
   uinttohexstr(eip, addrbuffer);
   int argc = 3;
   char **args = malloc(sizeof(char*)*(argc+1));  // args includes trailing null str
   char *debug_path = "/sys/debug.elf";
   args[2] = malloc(strlen(addrbuffer)+1);
   strcpy(args[2], addrbuffer);
   args[1] = malloc(strlen(exe_path)+1);
   strcpy(args[1], exe_path);
   args[0] = malloc(strlen(debug_path)+1);
   args[argc] = NULL;
   strcpy(args[0], debug_path);
   wm_queue_launch(debug_path, true, args, argc, true, false);
}

void show_endtask_dialog(int task_id, uint32_t process_uid, uint16_t int_no) {
   int popup = windowmgr_add();
   char buffer[50];
   sprintf(buffer, "Task %i paused due to exception %i", task_id, int_no);
   window_popup_dialog_t *dialog = window_popup_dialog(getWindow(popup), NULL, buffer);
   dialog->callback_func = &endtask_callback;
   dialog->dismiss_func = &endtask_dismiss; // closing dialog ends task
   dialog->wo_okbtn->x = 75;
   dialog->process_uid = process_uid;
   dialog->task_id = task_id;
   window_create_button(getWindow(popup), 135, 45, "Debug", &endtask_debug);
   if(get_task_window(task_id) >= 0 && gettasks()[task_id].process->uid == process_uid)
      window_disable(getWindow(get_task_window(task_id)));
   strcpy(getWindow(popup)->title, "Error");
   strcpy(dialog->wo_okbtn->text, "Exit");
   window_draw_outline(getWindow(popup), false);
   windowmgr_draw();
}
