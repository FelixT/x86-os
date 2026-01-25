#include <stdint.h>
#include <stdbool.h>
#include "../lib/string.h"
#include "lib/stdio.h"
#include "lib/sort.h"
#include "lib/dialogs.h"
#include "lib/ui/ui_mgr.h"
#include "lib/ui/ui_button.h"
#include "lib/ui/ui_input.h"
#include "lib/ui/ui_canvas.h"

#include "prog.h"
#include "prog_bmp.h"

volatile uint16_t *framebuffer;
volatile uint32_t width;
volatile uint32_t height;
uint16_t *file_icon;
uint16_t *folder_icon;
volatile int offset;
fs_dir_content_t *dir_content;
int shown_items;
int scrollable_height = 0;

char cur_path[512];
surface_t surface;
ui_mgr_t *ui;
wo_t *wo_menu;
wo_t *wo_path;
wo_t *wo_newfile;
wo_t *wo_grid = NULL;
wo_t *addnew_menu = NULL;

char tolower_c(char c) {
   if(c >= 'A' && c <= 'Z')
      c += ('a'-'A');
   return c;
}

int sort_func(const void *v1, const void *v2) {
   const fs_dir_entry_t *d1 = (const fs_dir_entry_t*)v1;
   const fs_dir_entry_t *d2 = (const fs_dir_entry_t*)v2;

   return strcmp(d1->filename, d2->filename);
}

void sort_dir() {
   if(!dir_content) return;
   sort(dir_content->entries, dir_content->size, sizeof(fs_dir_entry_t), sort_func);
}

void get_abs_path(char *out, char *inpath) {
   if(inpath[0] == '/') {
      // absolute path
      strcpy(out, inpath);
   } else {
      // relative path
      strcpy(out, cur_path);
      if(!strequ(out, "/"))
         strcat(out, "/");
      strcat(out, inpath);
   }
}

int entry_clicked(wo_t *grid, int window, int row, int col);

void display_items() {
   clear();
   
   int shown = 0;
   for(int i = 0; i < dir_content->size; i++) {
      fs_dir_entry_t *entry = &dir_content->entries[i];
      bool dotentry = entry->filename[0] == '.';
      if(entry->hidden && !dotentry)
         continue;

      shown++;
   }

   int grid_height = shown * 27;

   wo_t *old_grid = wo_grid;
   wo_t *grid = create_grid(0, -ui->scrolled_y, get_width(), grid_height, shown, 1);
   wo_grid = grid;

   // destroy existing
   if(old_grid)
      destroy_wo(old_grid);

   ui_add(ui, grid);
   grid_t *grid_data = grid->data;
   grid_data->click_func = &entry_clicked;
   grid_data->bordered = false;

   int row = 0;
   for(int i = 0; i < dir_content->size; i++) {
      fs_dir_entry_t *entry = &dir_content->entries[i];
      bool dotentry = entry->filename[0] == '.';
      if(entry->hidden && !dotentry)
         continue;
      wo_t *label = create_label(25, 2, 85, 23, entry->filename);
      label_t *label_data = label->data;
      label_data->padding_left = 5;
      label_data->halign = false;
      label_data->bordered = false;
      grid_add(grid, label, row, 0);

      wo_t *icon = create_image(4, 3, 20, 20, NULL);
      image_t *icon_data = icon->data;
      grid_add(grid, icon, row, 0);
      
      // draw
      if(entry->type == FS_TYPE_DIR) {
         icon_data->data = folder_icon;
         //bmp_draw((uint8_t*)folder_icon, x, y, 1, true);
      } else {
         icon_data->data = file_icon;

         uint32_t size = entry->file_size;
         char type[4];
         strcpy(type, "b");
         if(size > 1000) {
            strcpy(type, "kb");
            size /= 1000;
            if(size > 1000) {
               strcpy(type, "mb");
               size /= 1000;
            }
         }
         char sizeStr[20];
         sprintf(sizeStr, "%u %s", size, type);
         label = create_label(115, 2, 85, 23, sizeStr);
         label_data = label->data;
         label_data->padding_left = 5;
         label_data->halign = false;
         label_data->colour_txt = rgb16(160, 160, 160);
         label_data->colour_txt_hover = label_data->colour_txt;
         label_data->bordered = false;
         grid_add(grid, label, row, 0);
      }

      row++;
   }

   ui_draw(ui);

   scrollable_height = grid->height + 20;
   set_content_height(scrollable_height, -1);

   input_t *path_data = (input_t *)wo_path->data;
   path_data->valign = true;
   set_input_text(wo_path, cur_path);

   ui_draw(ui);
}

void path_callback() {
   input_t *path_data = (input_t *)wo_path->data;

   scroll_to(0, -1);

   char path[500];
   strcpy(path, path_data->text);
   int len = strlen(path);
   if(len > 0 && path[len-1] != '/') {
      strcat(path, "/");
   }

   fs_dir_content_t *content = read_dir(path);
   if(!content) {
      char buffer[500];
      sprintf(buffer, "Location '%s' not found", path);
      dialog_msg("Error", buffer);
   } else {
      strcpy(cur_path, path_data->text);
      dir_content = read_dir(cur_path);
      sort_dir();
      offset = 0;
      display_items();
      free(content, sizeof(fs_dir_content_t) * content->size);
   }

   end_subroutine();
}

void click(int x, int y) {

   if(ui->default_menu && ui->default_menu->visible) {
      ui_click(ui, x, y);
      ui_draw(ui);
      redraw();
      end_subroutine();
      return;
   }

   // clicked outside menu while its visible
   if(addnew_menu && addnew_menu->visible && !wo_newfile->hovering) {
      ui_click(ui, x, y);
      addnew_menu->visible = false;
      clear();
      ui_draw(ui);
      redraw();
      end_subroutine();
      return;
   }

   // clicked scrollbar
   if(x > get_width()) {
      end_subroutine();
      return;
   }

   ui_click(ui, x, y);

   end_subroutine();
}

int entry_clicked(wo_t *grid, int window, int row, int col) {
   (void)grid;
   (void)col;
   (void)window;
   int dir_index = -1;
   int i_pos = 0;

   // get index within dir
   for(int i = 0; i < dir_content->size; i++) {
      fs_dir_entry_t *entry = &dir_content->entries[i];
      bool dotentry = entry->filename[0] == '.';
      if(entry->hidden && !dotentry)
         continue;
      if(i_pos == row) {
         dir_index = i;
         break;
      }
      i_pos++;
   }
   

   fs_dir_entry_t *clicked_entry = &dir_content->entries[dir_index];

   if(clicked_entry->type == FS_TYPE_DIR) {
      offset = 0;
      scroll_to(0, -1);

      // update path
      if(clicked_entry->filename[0] == '.') {
         if(clicked_entry->filename[1] == '.') {
            int lastslashpos = -1;
            for(int i = 0; cur_path[i] != '\0'; i++) {
               if(cur_path[i] == '/')
                  lastslashpos = i;
            }

            if(lastslashpos > 0) {
               cur_path[lastslashpos] = '\0';
            } else if(lastslashpos == 0) {
               cur_path[1] = '\0';
            }
         } else {
            // do nothing
         }
      } else {
         int pi = strlen(cur_path);
         if(pi == 1) pi = 0;
         cur_path[pi] = '/';
         cur_path[pi+1] = '\0';
         strcat(cur_path, clicked_entry->filename);
      }

      // read dir
      dir_content = read_dir(cur_path);
      sort_dir();
      display_items();

      redraw();

   } else {
      // file
      char extension[4];
      strsplit(NULL, extension, clicked_entry->filename, '.');

      char fullpath[255];
      int pi = strlen(cur_path);
      for(int x = 0; x < pi; x++)
         fullpath[x] = cur_path[x];
      fullpath[pi] = '/';
      fullpath[pi+1] = '\0';
      strcat(fullpath, clicked_entry->filename);

      bool launched = false;
      // handle supported extensions
      // bmp, elf, txt
      if(strequ(extension, "bmp")) {
         char **args = (char**)malloc(2*sizeof(char*));
         char elfpath[32] = "/sys/bmpview.elf";
         args[0] = malloc(strlen(elfpath)+1);
         strcpy(args[0], elfpath);
         args[1] = malloc(strlen(fullpath)+1);
         strcpy(args[1], fullpath);

         // note: this also ends the subroutine
         launched = true;
         launch_task(elfpath, 2, args, false);
      }

      if(strequ(extension, "elf")) {
         // note: this also ends the subroutine
         launched = true;
         launch_task(fullpath, 0, NULL, false);
      }

      if(strequ(extension, "txt") || strequ(extension, "c")) {
         char **args = (char**)malloc(1*sizeof(char*));
         args[0] = malloc(strlen(fullpath)+1);
         strcpy(args[0], fullpath);

         // note: this also ends the subroutine
         launched = true;
         launch_task("/sys/text.elf", 1, args, false);
      }

      if(strequ(extension, "fon")) {
         set_setting(SETTING_SYS_FONT_PATH, (uint32_t)fullpath);
         clear();
         display_items();
      }

      if(launched) {
         debug_println("This should never happen!"); // launching task should end the subroutine
         while(true) {}
      }

   }

   return 1;
}

void resize(uint32_t fb, uint32_t w, uint32_t h) {
   (void)w;
   framebuffer = (uint16_t*)fb;
   surface = get_surface();
   width = surface.width;
   height = h;
   ui->surface = &surface;

   wo_menu->width = w;
   wo_menu->y = height - 20;
   int btn_width = wo_newfile->width + 2 + 5;
   wo_path->width = w - btn_width;
   wo_newfile->x = w - wo_newfile->width - 2;
   if(addnew_menu) {
      addnew_menu->x = wo_menu->width - 70;
      addnew_menu->y = wo_menu->y - 40;
   }

   display_items();
   end_subroutine();
}

void scroll(int deltaY, int offsetY, int window) {
   (void)window;
   // clear menu
   draw_context_t context = ui_get_context(ui);
   context.clipRect.height -= wo_menu->height;
   ui_scroll_buffer_c(ui, deltaY, context);
   wo_grid->y -= deltaY;
   wo_grid->draw_func(wo_grid, context);
   wo_menu->draw_func(wo_menu, ui_get_context(ui));
   offset = offsetY/27;
   end_subroutine();
}

void add_file_callback(char *filename) {
   if(filename && !strequ(filename, "")) {
      char path[512];
      get_abs_path(path, filename);
      debug_println("Creating file %s", path);
      int fd = new_file(path);
      if(fd < 0) {
         char buffer[250];
         sprintf(buffer, "Failed to create file '%s'", path);
         dialog_msg("Error", buffer);
      } else {
         close(fd);
      }
      // refresh
      dir_content = read_dir(cur_path);
      sort_dir();
      display_items();
      redraw();
   }
}

void add_file() {
   if(addnew_menu && addnew_menu->visible) {
      addnew_menu->visible = false;
      display_items();
      ui_draw(ui);
   }
   dialog_input("Enter filename", (void*)&add_file_callback);
}

void add_folder_callback(char *name) {
   if(name && !strequ(name, "")) {
      char path[512];
      get_abs_path(path, name);
      debug_println("Creating folder %s", path);
      // actually add the dir
      if(!mkdir(path)) {
         char buffer[250];
         sprintf(buffer, "Failed to create folder '%s'", path);
         dialog_msg("Error", buffer);
      }
      // refresh
      dir_content = read_dir(cur_path);
      sort_dir();
      display_items();
      redraw();
   }
}

void add_folder() {
   if(addnew_menu && addnew_menu->visible) {
      addnew_menu->visible = false;
      display_items();
      ui_draw(ui);
      redraw();
   }
   dialog_input("Enter folder name", &add_folder_callback);
}

void keypress(uint16_t c, int window) {
   (void)window;

   bool uparrow = c == 0x100;
   bool downarrow = c == 0x101;

   if(uparrow || downarrow) {
      int scroll_offset = ui->scrolled_y;
      if(uparrow)
         scroll_offset-=15;
      else
         scroll_offset+=15;

      if(scroll_offset < 0) offset = 0;
      if(scroll_offset >= scrollable_height - ui->surface->height) offset = scrollable_height - ui->surface->height;

      scroll_to(scroll_offset, -1);
   }

   ui_keypress(ui, c);

   end_subroutine();

}

void release(int x, int y, int window) {
   (void)window;
   ui_release(ui, x, y);
   end_subroutine();
}

void hover(int x, int y) {
   ui_hover(ui, x, y);
   draw_context_t context = ui_get_context(ui);
   wo_menu->draw_func(wo_menu, context);
   redraw();
   end_subroutine();
}

int rightclicked_index = -1;

void rightclick(int x, int y) {
   bool menu_visible = ui->default_menu && ui->default_menu->visible;

   if(ui->default_menu) {
      // see where we clicked to determine if we can open/rename
      int position = y/27+offset;

      if(dir_content == NULL) end_subroutine();

      int index = -1;
      int i_pos = 0;
      for(int i = 0; i < dir_content->size; i++) {
         fs_dir_entry_t *entry = &dir_content->entries[i];
         bool dotentry = entry->filename[0] == '.';
         if(entry->hidden && !dotentry)
            continue;
         if(i_pos == position)
            index = i;
         i_pos++;
      }
      rightclicked_index = index;
      menu_t *menu = ui->default_menu->data;
      menu->items[0].enabled = index > -1;
      menu->items[1].enabled = index > -1;
   }
   if(menu_visible) {
      display_items();
      ui->default_menu->visible = false;
      ui_rightclick(ui, x, y);
   } else {
      display_items();
      ui_draw(ui);
      ui_rightclick(ui, x, y);
   }
   redraw();
   end_subroutine();
}

void mouseout() {
   ui_hover(ui, -1, -1);
   ui_draw(ui);
   end_subroutine();
}

void rename_callback(char *out) {
   fs_dir_entry_t *entry = &dir_content->entries[rightclicked_index];
   char buffer[256];
   strcpy(buffer, cur_path);
   if(!strendswith(buffer, "/"))
      strcat(buffer, "/");
   strcat(buffer, entry->filename);
   debug_println("Renaming %s to %s", buffer, out);
   rename(buffer, out);
   // refresh
   dir_content = read_dir(cur_path);
   sort_dir();
   clear();
   display_items();
}

void rename_menuclick(wo_t *item, int index, int window) {
   (void)item;
   (void)index;
   (void)window;
   char dialog[256];
   fs_dir_entry_t *entry = &dir_content->entries[rightclicked_index];
   sprintf(dialog, "Rename %s '%s'", entry->type == FS_TYPE_DIR ? "folder" : "file", entry->filename);
   dialog_input(dialog, &rename_callback);
}

void open_menuclick(wo_t *item, int index, int window) {
   (void)index;
   (void)window;
   item->visible = false;
   click(item->x, item->y);
}

void settings() {
   dialog_window_settings(-1, "File Manager");
}

void quit_callback() {
   close_window(-1);
   exit(0);
}

void quit() {
   dialog_yesno("Quit Files", "Are you sure you want to quit files", &quit_callback);
}

void show_add_menu() {
   if(!addnew_menu) {
      wo_t *menu = create_menu(wo_menu->width - 70, wo_menu->y - 40, 70, 40);
      add_menu_item(menu, "New file", (void*)&add_file);
      add_menu_item(menu, "New folder", (void*)&add_folder);
      ui_add(ui, menu);
      ui_draw(ui);
      addnew_menu = menu;
   } else {
      addnew_menu->visible = !addnew_menu->visible;
      if(addnew_menu->visible) {
         addnew_menu->x = wo_menu->width - 70;
         addnew_menu->y = wo_menu->y - 40;
      }
      ((menu_t*)addnew_menu->data)->selected_index = -1;
      display_items();
      ui_draw(ui);
   }
}

void _start(int argc, char **args) {
   (void)argc;
   (void)args;

   // init
   set_window_size(315, 235);
   set_window_title("File Manager");

   surface = get_surface();
   ui = ui_init(&surface, -1);

   dir_content = read_dir("/");
   sort_dir();
   offset = 0;
   
   file_icon = dialog_load_icon("/bmp/file20.bmp", NULL, NULL);
   if(!file_icon) {
      write_str("File icon not found\n");
      exit(0);
   }
   folder_icon = dialog_load_icon("/bmp/folder20.bmp", NULL, NULL);
   if(!folder_icon) {
      write_str("Folder icon not found\n");
      exit(0);
   }

   framebuffer = (uint16_t*)(get_surface().buffer);

   create_scrollbar(&scroll, -1);
   override_keypress((uint32_t)&keypress, -1);
   override_click((uint32_t)&click, -1);
   override_draw((uint32_t)NULL);
   override_resize((uint32_t)&resize, -1);
   override_release((uint32_t)&release, -1);
   override_hover((uint32_t)&hover, -1);
   override_rightclick((uint32_t)&rightclick, -1);
   override_mouseout((uint32_t)&mouseout, -1);
   width = get_surface().width;
   height = get_height();

   int displayedwidth = get_width();

   wo_menu = create_canvas(0, height - 20, displayedwidth, 20);
   wo_menu->fixed = true;
   canvas_t *menu_data = (canvas_t *)wo_menu->data;
   menu_data->bordered = false;

   strcpy(cur_path, "/");
   int x = 4;
   int y = 2;
   
   wo_path = create_input(x, y, displayedwidth - 45 - 4 - 5, 16);
   set_input_text(wo_path, cur_path);
   input_t *path_data = (input_t *)wo_path->data;
   path_data->return_func = &path_callback;
   canvas_add(wo_menu, wo_path);
   x += wo_path->width + 2;

   wo_newfile = create_button(x, y, 45, 16, "Add");
   button_t *newfile_data = (button_t *)wo_newfile->data;
   newfile_data->release_func = (void *)&show_add_menu;
   canvas_add(wo_menu, wo_newfile);
   x += wo_newfile->width + 2;

   display_items();
   ui_add(ui, wo_menu);
   ui_draw(ui);

   redraw();

   // setup rightclick menu
   ui->default_menu = create_menu(0, 0, 80, 90);
   ui->default_menu->visible = false;
   add_menu_item(ui->default_menu, "Open", &open_menuclick);
   add_menu_item(ui->default_menu, "Rename", &rename_menuclick);
   add_menu_item(ui->default_menu, "New file", (void*)&add_file);
   add_menu_item(ui->default_menu, "New folder", (void*)&add_folder);
   add_menu_item(ui->default_menu, "Settings", (void*)&settings);
   add_menu_item(ui->default_menu, "Quit", (void*)&quit);
   resize_menu(ui->default_menu);

   // main program loop
   while(1 == 1) {
      //for(int i = 0; i < (int)width; i++)
      //framebuffer[i] = 0;
      //asm volatile("pause");
      yield();
   }

   exit(0);

}