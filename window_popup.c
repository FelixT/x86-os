#include <stddef.h>
#include "window_popup.h"
#include "windowmgr.h"
#include "window.h"
#include "windowobj.h"
#include "lib/string.h"
#include "memory.h"

// window popup - subwindows

void window_popup_init(gui_window_t *window, gui_window_t *parent) {
   window->keypress_func = NULL;
   window->draw_func = NULL;
   if(parent != NULL)
      strcpy(window->title, parent->title); // take name from parent
}

// define default popups - common mini-progs

void window_popup_dialog_close(void *windowobj, void *regs) {
   (void)regs;
   gui_window_t *window = getSelectedWindow();
   window_popup_dialog_t *dialog = (window_popup_dialog_t*)window->state;
   int index = get_window_index_from_pointer(window);
   int parent_index = get_window_index_from_pointer(dialog->parent);
   // store output if exists
   windowobj_t *wo = (windowobj_t*)windowobj;
   bool ok_clicked = strcmp(wo->text, "Ok");
   char *output = NULL;
   if(ok_clicked && dialog->callback_func) {
      if(dialog->wo_output) {
         output = malloc(strlen(dialog->wo_output->text));
         strcpy(output, dialog->wo_output->text);
         debug_printf("Return string %s\n", output);
      }
      
   }
   // self destruct
   window_close(NULL, index);
   setSelectedWindowIndex(parent_index);

   // launch callback if exists
   if(dialog->callback_func == NULL || !ok_clicked)
      return;

   if(get_task_from_window(getSelectedWindowIndex()) == -1) {
      // call as kernel
      dialog->callback_func(output);
      free((uint32_t)output, strlen(output));
      getSelectedWindow()->needs_redraw = true;
      window_draw(getSelectedWindow());
   } else {
      if(!gui_interrupt_switchtask(regs))
         return;
      // calling function as task
      uint32_t *args = malloc(sizeof(uint32_t) * 1);
      args[0] = (uint32_t)output;
      uint32_t start_page = (uint32_t)output / 0x1000;
      uint32_t end_page = ((uint32_t)output + strlen(output) + 0xFFF) / 0x1000;
      for(uint32_t i = start_page; i < end_page; i++)
         map(page_get_current(), i*0x1000, i*0x1000, 1, 1);
      map(page_get_current(), (uint32_t)args, (uint32_t)args, 1, 1);
   
      task_call_subroutine(regs, "popupreturn", (uint32_t)dialog->callback_func, args, 1);
   
      getSelectedWindow()->needs_redraw = true;
      window_draw(getSelectedWindow());
   }
}

// void return_func(char *output)
void window_popup_dialog(gui_window_t *window, gui_window_t *parent, char *text, bool output, void *return_func) {
   if(parent != NULL)
      parent->children[parent->child_count++] = window;
   
   int height = 95;
   if(output)
      height += 30;
   
   window_resize(NULL, window, 260, height);

   window_popup_init(window, parent);
   // add default window objects

   strcpy(window->title, "Dialog");
   if(parent != NULL) {
      window->x = parent->x + 50;
      window->y = parent->y + 50;
   }
   if(window->y + window->height > (int)gui_get_height())
      window->y = gui_get_height() - window->height;

   window_popup_dialog_t *dialog = malloc(sizeof(window_popup_dialog_t));
   dialog->parent = parent;
   dialog->wo_output = NULL;
   dialog->callback_func = return_func;
   window->state = (void*)dialog;
   window->state_size = sizeof(window_popup_dialog_t);
   window->resizable = false;

   // dialog message
   int y = 15;
   windowobj_t *wo_msg = window_create_text(window, 15, y, text);
   wo_msg->width = 220;
   wo_msg->height = 20;
   wo_msg->disabled = true;

   if(output) {
      y += 30;
      dialog->wo_output = window_create_text(window, 15, y, "");
      dialog->wo_output->width = 140;
   }

   // ok button
   y += 30;
   windowobj_t *wo_okbtn = window_create_button(window, 15, y, "Ok", &window_popup_dialog_close);

   if(output) {
      windowobj_t *wo_cancelbtn = window_create_button(window, 15 + wo_okbtn->width + 10, y, "Cancel", &window_popup_dialog_close);
   }

}

void window_popup_filepicker_click(void *windowobj, void *regs, int relX, int relY) {
   (void)regs;
   (void)relX;
   (void)relY;
   windowobj_t *wo = (windowobj_t*)windowobj;
   window_popup_filepicker_t *fp = (window_popup_filepicker_t*)getSelectedWindow()->state;
   if(strcmp(wo->text, ".")) {
      // do nothing
   } else if(strcmp(wo->text, "..")) {
      strsplit_last(fp->wo_path->text, NULL, fp->wo_path->text, '/');
      strsplit_last(fp->wo_path->text, NULL, fp->wo_path->text, '/');
      strcat(fp->wo_path->text, "/");
      if(strcmp(fp->wo_path->text, "/")) {
         fp->currentdir = NULL;
      } else {
         fat_dir_t *items = malloc(sizeof(fat_dir_t)*fat_get_dir_size(fp->currentdir->firstClusterNo));
         fat_read_dir(fp->currentdir->firstClusterNo, items);
         memcpy_fast(fp->currentdir, &(items[1]), sizeof(fat_dir_t));
         free((uint32_t)items, sizeof(fat_dir_t)*fat_get_dir_size(fp->currentdir->firstClusterNo));
      }
   } else {
      fp->currentdir = fat_follow_path_chain(wo->text, fp->currentdir);
      strcat(fp->wo_path->text, wo->text);
      if((fp->currentdir->attributes & 0x10) == 0x10) // dir
         strcat(fp->wo_path->text, "/");
   }

   if(fp->currentdir != NULL && (fp->currentdir->attributes & 0x10) == 0) {
      // clicked on file
      int fp_window = getSelectedWindowIndex();

      setSelectedWindowIndex(get_window_index_from_pointer(fp->parent));

      void (*callback)(char *) = fp->callback_func;
      char *path = malloc(256);
      strcpy(path, fp->wo_path->text);

      // self destruct
      window_close(NULL, fp_window);

      if(get_task_from_window(getSelectedWindowIndex()) == -1) {
         // call as kernel
         callback(path);
         free((uint32_t)path, 256);
         getSelectedWindow()->needs_redraw = true;
         window_draw(getSelectedWindow());
      } else {
         if(!gui_interrupt_switchtask(regs)) return;
         // calling function as task
         uint32_t *args = malloc(sizeof(uint32_t) * 1);
         args[0] = (uint32_t)path;
         map(page_get_current(), (uint32_t)path, (uint32_t)path, 1, 1);
         map(page_get_current(), (uint32_t)args, (uint32_t)args, 1, 1);
      
         task_call_subroutine(regs, "filepickerreturn", (uint32_t)callback, args, 1);
      
         getSelectedWindow()->needs_redraw = true;
         window_draw(getSelectedWindow());

         return;
      }

      return;
   }

   // remove existing windowobjs except path and scrollbar
   gui_window_t *window = getSelectedWindow();
   for(int i = 2; i < window->window_object_count; i++) {
      free((uint32_t)window->window_objects[i], sizeof(windowobj_t));
   }
   window->window_object_count = 2;

   window_clearbuffer(window, window->bgcolour);

   fat_dir_t *items;
   int size;
   if(fp->currentdir != NULL && fp->currentdir->firstClusterNo > 0) {
      size = fat_get_dir_size(fp->currentdir->firstClusterNo);
      items = malloc(sizeof(fat_dir_t)*size);
      fat_read_dir(fp->currentdir->firstClusterNo, items);
   } else {
      size = fat_get_bpb().noRootEntries;
      items = fat_read_root();
      strcpy(fp->wo_path->text, "/");
      fp->currentdir = NULL;
   }
   int width = window->width - 2;
   int y = 18;
   for(int i = 0; i < size; i++) {
      if(items[i].filename[0] == 0) break;
      if(i == 0) continue;
      bool hidden = (items[i].attributes & 0x02) == 0x02;
      bool dotentry = items[i].filename[0] == 0x2E;
      if(hidden & !dotentry) continue; // ignore

      windowobj_t *wo_file = malloc(sizeof(windowobj_t));
      windowobj_init(wo_file, &window->surface);
      wo_file->type = WO_BUTTON;
      wo_file->x = 1;
      wo_file->y = y;
      wo_file->width = width;
      wo_file->height = 16;
      wo_file->text = malloc(13);
      // filename
      strcpy_fixed(wo_file->text, (char*)items[i].filename, 8);
      wo_file->text[8] = '\0';
      if(strchr(wo_file->text, ' '))
         strchr(wo_file->text, ' ')[0] = '\0';
      // extension
      if(items[i].filename[8] != ' ') {
         strcat(wo_file->text, ".");
         strcpy_fixed(wo_file->text+strlen(wo_file->text), (char*)&(items[i].filename[8]), 3);
         if(strchr(wo_file->text, ' '))
            strchr(wo_file->text, ' ')[0] = '\0';
      }
      wo_file->release_func = &window_popup_filepicker_click;
      wo_file->texthalign = false;
      window->window_objects[window->window_object_count++] = wo_file;

      y+=17;
   }
   window_set_scrollable_height(regs, window, y);
   if(window->scrollbar->visible) {
      for(int i = 2; i < window->window_object_count; i++) {
         window->window_objects[i]->width -= 14; // scrollbar width
      }
   }
   window->window_objects[1]->width = width - (window->scrollbar->visible ? 14 : 0);

   //free((uint32_t)items, sizeof(fat_dir_t*)*fat_get_dir_size(fp->currentdir->firstClusterNo));

}

void window_popup_filepicker_free(void *window) {
   gui_window_t *w = (gui_window_t*)window;
   window_popup_filepicker_t *fp = (window_popup_filepicker_t*)w->state;
   free((uint32_t)fp->currentdir, sizeof(fat_dir_t));
   free((uint32_t)fp, sizeof(window_popup_filepicker_t));
   w->state = NULL;
}

window_popup_filepicker_t *window_popup_filepicker(gui_window_t *window, gui_window_t *parent, void *callback) {
   parent->children[parent->child_count++] = window;

   window_resize(NULL, window, 244, 200);
   window_popup_init(window, parent);
   strcpy(window->title, "File Browser");

   window->x = parent->x + 50;
   window->y = parent->y + 50;
   if(window->y + window->height > (int)gui_get_height())
      window->y = gui_get_height() - window->height;
   int width = window->width - 2;

   window_create_scrollbar(window, NULL);

   window->state_size = sizeof(window_popup_filepicker_t);
   window->state = malloc(window->state_size);
   window->state_free = &window_popup_filepicker_free;
   window_popup_filepicker_t *fp = (window_popup_filepicker_t*)window->state;
   fp->currentdir = NULL;
   fp->callback_func = callback;
   fp->parent = parent;
   int y = 1;
   fp->wo_path = malloc(sizeof(windowobj_t));
   windowobj_init(fp->wo_path, &window->surface);
   fp->wo_path->type = WO_TEXT;
   fp->wo_path->x = 1;
   fp->wo_path->y = y;
   fp->wo_path->width = width;
   fp->wo_path->height = 16;
   fp->wo_path->texthalign = false;
   fp->wo_path->disabled = true;
   fp->wo_path->text = malloc(100);
   strcpy(fp->wo_path->text, "/");
   window->window_objects[window->window_object_count++] = fp->wo_path;
   y+=17;

   // display root
   fat_dir_t *items = fat_read_root();
   for(int i = 0; i < fat_get_bpb().noRootEntries; i++) {
      if(items[i].filename[0] == 0) break;
      if(i == 0) continue;
      bool hidden = (items[i].attributes & 0x02) == 0x02;
      bool dotentry = items[i].filename[0] == 0x2E;
      if(hidden & !dotentry) continue; // ignore

      windowobj_t *wo_file = malloc(sizeof(windowobj_t));
      windowobj_init(wo_file, &window->surface);
      wo_file->type = WO_BUTTON;
      wo_file->x = 1;
      wo_file->y = y;
      wo_file->width = width;
      wo_file->height = 16;
      wo_file->text = malloc(13);
      wo_file->text[12] = '\0';
      // filename
      strcpy_fixed(wo_file->text, (char*)items[i].filename, 8);
      wo_file->text[8] = '\0';
      if(strchr(wo_file->text, ' '))
         strchr(wo_file->text, ' ')[0] = '\0';
      // extension
      if(items[i].filename[8] != ' ') {
         strcat(wo_file->text, ".");
         strcpy_fixed(wo_file->text+strlen(wo_file->text), (char*)&(items[i].filename[8]), 3);
         if(strchr(wo_file->text, ' '))
            strchr(wo_file->text, ' ')[0] = '\0';
      }
      wo_file->release_func = &window_popup_filepicker_click;
      wo_file->texthalign = false;
      window->window_objects[window->window_object_count++] = wo_file;

      y+=17;
   }
   window_set_scrollable_height(NULL, window, y);
   if(window->scrollbar->visible) {
      for(int i = 2; i < window->window_object_count; i++) {
         window->window_objects[i]->width -= 14; // scrollbar width
      }
   }
   window->window_objects[1]->width = width - (window->scrollbar->visible ? 14 : 0);

   free((uint32_t)items, sizeof(fat_dir_t)*fat_get_bpb().noRootEntries);

   return fp;
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
window_popup_colourpicker_t *window_popup_colourpicker(gui_window_t *window, gui_window_t *parent, void *callback) {
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
   strcpy(wo_col->text, "FFFFFF");
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