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
   strcpy(window->title, parent->title); // take name from parent
}

// define default popups - common mini-progs

void window_popup_dialog(gui_window_t *window, gui_window_t *parent, char *text) {
   window_resize(NULL, window, 260, 95);

   window_popup_init(window, parent);
   // add default window objects

   strcpy(window->title, "Dialog");

   // dialog text
   int y = 15;
   windowobj_t *wo_txt = malloc(sizeof(windowobj_t));
   windowobj_init(wo_txt, &window->surface);
   wo_txt->type = WO_TEXT;
   wo_txt->x = 15;
   wo_txt->y = y;
   wo_txt->width = 220;
   wo_txt->height = 20;
   wo_txt->text = malloc(500);
   strcpy(wo_txt->text, text);
   //d_bgcolour_wo->return_func = &window_settings_set_bgcolour;
   window->window_objects[window->window_object_count++] = wo_txt;

   // ok button
   y += 30;
   windowobj_t *wo_okbtn = malloc(sizeof(windowobj_t));
   windowobj_init(wo_okbtn, &window->surface);
   wo_okbtn->type = WO_BUTTON;
   wo_okbtn->x = 15;
   wo_okbtn->y = y;
   wo_okbtn->width = 110;
   wo_okbtn->height = 20;
   wo_okbtn->text = malloc(strlen("OK") + 1);
   strcpy(wo_okbtn->text, "OK");
   //d_bgcolour_wo->return_func = &window_settings_set_bgcolour;
   window->window_objects[window->window_object_count++] = wo_okbtn;

   // cancel button
   windowobj_t *wo_cancelbtn = malloc(sizeof(windowobj_t));
   windowobj_init(wo_cancelbtn, &window->surface);
   wo_cancelbtn->type = WO_BUTTON;
   wo_cancelbtn->x = 130;
   wo_cancelbtn->y = y;
   wo_cancelbtn->width = 110;
   wo_cancelbtn->height = 20;
   wo_cancelbtn->text = malloc(strlen("Cancel") + 1);
   strcpy(wo_cancelbtn->text, "Cancel");
   //d_bgcolour_wo->return_func = &window_settings_set_bgcolour;
   window->window_objects[window->window_object_count++] = wo_cancelbtn;

}

void window_popup_filepicker_click(void *windowobj) {
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
      // self destruct
      window_close(NULL, getSelectedWindowIndex());

      // clicked on file
      setSelectedWindowIndex(get_window_index_from_pointer(fp->parent));
      fp->callback_func(fp->wo_path->text);
      return;
   }

   // remove existing windowobjs except path
   gui_window_t *window = getSelectedWindow();
   for(int i = 1; i < window->window_object_count; i++)
      free((uint32_t)window->window_objects[i], sizeof(windowobj_t));
   window->window_object_count = 1;

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
   int y = 18;
   for(int i = 0; i < size; i++) {
      if(items[i].filename[0] == 0) break;
      bool hidden = (items[i].attributes & 0x02) == 0x02;
      bool dotentry = items[i].filename[0] == 0x2E;
      if(hidden & !dotentry) continue; // ignore

      windowobj_t *wo_file = malloc(sizeof(windowobj_t));
      windowobj_init(wo_file, &window->surface);
      wo_file->type = WO_BUTTON;
      wo_file->x = 1;
      wo_file->y = y;
      wo_file->width = 190;
      wo_file->height = 16;
      wo_file->text = malloc(13);
      // filename
      strcpy_fixed(wo_file->text, items[i].filename, 8);
      wo_file->text[8] = '\0';
      if(strchr(wo_file->text, ' '))
         strchr(wo_file->text, ' ')[0] = '\0';
      // extension
      if(items[i].filename[8] != ' ') {
         strcat(wo_file->text, ".");
         strcpy_fixed(wo_file->text+strlen(wo_file->text), (char*)&(items[i].filename[8]), 3);
         if(strchr(wo_file->text, ' '))
            strchr(wo_file->text, ' ')[0] = '\0';
      }      wo_file->click_func = &window_popup_filepicker_click;
      wo_file->texthalign = false;
      window->window_objects[window->window_object_count++] = wo_file;

      y+=17;
   }

   //free((uint32_t)items, sizeof(fat_dir_t*)*fat_get_dir_size(fp->currentdir->firstClusterNo));

}

window_popup_filepicker_t *window_popup_filepicker(gui_window_t *window, gui_window_t *parent, void *callback) {
   window_resize(NULL, window, 200, 160);
   window_popup_init(window, parent);
   strcpy(window->title, "File Browser");

   window->state = malloc(sizeof(window_popup_filepicker_t));
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
   fp->wo_path->width = 190;
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
      bool hidden = (items[i].attributes & 0x02) == 0x02;
      bool dotentry = items[i].filename[0] == 0x2E;
      if(hidden & !dotentry) continue; // ignore

      windowobj_t *wo_file = malloc(sizeof(windowobj_t));
      windowobj_init(wo_file, &window->surface);
      wo_file->type = WO_BUTTON;
      wo_file->x = 1;
      wo_file->y = y;
      wo_file->width = 190;
      wo_file->height = 16;
      wo_file->text = malloc(13);
      wo_file->text[12] = '\0';
      // filename
      strcpy_fixed(wo_file->text, items[i].filename, 8);
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
      wo_file->click_func = &window_popup_filepicker_click;
      wo_file->texthalign = false;
      window->window_objects[window->window_object_count++] = wo_file;

      y+=17;
   }

   free((uint32_t)items, sizeof(fat_dir_t)*fat_get_bpb().noRootEntries);

   return fp;
}