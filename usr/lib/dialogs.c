#include "dialogs.h"

#include "../prog.h"
#include "stdio.h"
#include "draw.h"
#include "sort.h"
#include "../../lib/string.h"
#include "../prog_bmp.h"

int dialog_count = 0;
dialog_t *dialogs[MAX_DIALOGS];

dialog_t *dialog_from_window(int window) {
   for(int i = 0; i < dialog_count; i++) {
      if(dialogs[i]->active && dialogs[i]->window == window)
         return dialogs[i];
   }
   return NULL;
}

int get_free_dialog() {
   for(int i = 0; i < dialog_count; i++) {
      if(dialogs[i]->active == false)
         return i;
   }
   if(dialog_count < MAX_DIALOGS) {
      dialog_t *dialog = malloc(sizeof(dialog_t));
      int index = dialog_count;
      dialogs[index] = dialog;
      dialog_count++;
      return index;
   } else {
      return -1;
   }
}

void dialog_click(int x, int y, int window) {
   dialog_t *dialog = dialog_from_window(window);
   if(!dialog) {
      debug_println("Couldn't find dialog for window %i\n", window);
      end_subroutine();
   }
   ui_click(dialog->ui, x, y);
   end_subroutine();
}

void dialog_release(int x, int y, int window) {
   dialog_t *dialog = dialog_from_window(window);
   if(!dialog) {
      debug_println("Couldn't find dialog for window %i\n", window);
      end_subroutine();
   }
   ui_release(dialog->ui, x, y);
   end_subroutine();
}

void dialog_hover(int x, int y, int window) {
   dialog_t *dialog = dialog_from_window(window);
   if(!dialog) {
      debug_println("Couldn't find dialog for window %i\n", window);
      end_subroutine();
   }
   ui_hover(dialog->ui, x, y);
   end_subroutine();
}

void dialog_close(wo_t *wo, int window) {
   (void)wo;
   dialog_t *dialog = dialog_from_window(window);
   if(!dialog) {
      debug_println("Can't find window %i", window);
      end_subroutine();
      return;
   }
   dialog->active = false;
   close_window(window);
}

void dialog_resize(uint16_t fb, int width, int height, int window) {
   (void)fb;
   (void)width;
   (void)height;
   dialog_t *dialog = dialog_from_window(window);
   if(!dialog) {
      debug_println("Can't find window %i", window);
      end_subroutine();
      return;
   }
   dialog->surface = get_surface_w(window);
   dialog->ui->surface = &dialog->surface;
   ui_draw(dialog->ui);
   end_subroutine();
}

void dialog_rightclick(int x, int y, int window) {
   dialog_t *dialog = dialog_from_window(window);
   if(!dialog) {
      debug_println("Couldn't find dialog for window %i\n", window);
      end_subroutine();
   }
   ui_rightclick(dialog->ui, x, y);
   end_subroutine();
}

void dialog_mouseout(int window) {
   dialog_t *dialog = dialog_from_window(window);
   if(!dialog) {
      debug_println("Couldn't find dialog for window %i\n", window);
      end_subroutine();
   }
   ui_hover(dialog->ui, -1, -1);
   end_subroutine();
}

void dialog_window_close(int window) {
   dialog_close(NULL, window);
   end_subroutine();
}

void dialog_keypress(int c, int window) {
   dialog_t *dialog = dialog_from_window(window);
   if(!dialog) {
      debug_println("Couldn't find dialog for window %i\n", window);
      end_subroutine();
   }
   ui_keypress(dialog->ui, c);
   if(c == 0x1B) {
      dialog_close(NULL, window);
   }
   if(!dialog->ui->focused) {
      if(c == 0x100) {
         dialog->ui->scrolled_y -= 15;
         if(dialog->ui->scrolled_y < 0)
            dialog->ui->scrolled_y = 0;
         scroll_to(dialog->ui->scrolled_y, window);
      } else if(c == 0x101) {
         dialog->ui->scrolled_y += 15;
         if(dialog->ui->scrolled_y > dialog->content_height - dialog->surface.height)
            dialog->ui->scrolled_y = dialog->content_height - dialog->surface.height;
         if(dialog->ui->scrolled_y < 0)
            dialog->ui->scrolled_y = 0;
         scroll_to(dialog->ui->scrolled_y, window);
      }
   }

   end_subroutine();
}

void dialog_set_title(dialog_t *dialog, char *title) {
   strcpy(dialog->title, title);
   set_window_title_w(dialog->window, title);
}

void dialog_defaultmenu_close(wo_t *menu, int item, int window) {
   (void)menu;
   (void)item;
   dialog_close(NULL, window);
}

void dialog_defaultmenu_settings(wo_t *menu, int item, int window) {
   (void)menu;
   (void)item;
   dialog_t *dialog = dialog_from_window(window);
   if(!dialog) {
      debug_println("Couldn't find dialog for window %i\n", window);
      end_subroutine();
   }
   dialog_window_settings(window, dialog->title);
}

void dialog_defaultmenu_minimise(wo_t *menu, int item, int window) {
   (void)menu;
   (void)item;
   set_window_minimised(true, window);
}

void dialog_init(dialog_t *dialog, int window) {
   dialog->window = window;
   dialog->surface = get_surface_w(window);
   dialog->ui = ui_init(&dialog->surface, window);
   dialog->active = true;
   dialog->callback = NULL;
   dialog->content_height = 0;
   map_init(&dialog->wo_map, 64);
   strcpy(dialog->title, "Window");
   dialog_init_overrides(window);
   if(window != -1) {
      // set coods to relative of main window
      coord_t mainw_pos = get_window_position(-1);
      set_window_position(mainw_pos.x + 30, mainw_pos.y + 30, window);
   }
   // setup default menu
   dialog->ui->default_menu = create_menu(0, 0, 105, 45);
   dialog->ui->default_menu->visible = false;
   add_menu_item(dialog->ui->default_menu, "Close", &dialog_defaultmenu_close);
   add_menu_item(dialog->ui->default_menu, "Minimise", &dialog_defaultmenu_minimise);
   add_menu_item(dialog->ui->default_menu, "Settings", &dialog_defaultmenu_settings);
   ui_add(dialog->ui, dialog->ui->default_menu);
}

void dialog_add(dialog_t *dialog, char *key, wo_t *wo) {
   map_insert(&dialog->wo_map, key, wo);
}

wo_t *dialog_get(dialog_t *dialog, char *key) {
   return (wo_t*)map_lookup(&dialog->wo_map, key);
}

bool dialog_msg(char *title, char *text) {
   int y = 10;
   int index = get_free_dialog();
   if(index < 0) return false;
   dialog_t *msg = dialogs[index];

   dialog_init(msg, create_window(260, 100));
   msg->type = DIALOG_MSG;

   wo_t *label_wo = create_label(15, y, 260 - 30, 20, text);
   ui_add(msg->ui, label_wo);

   y += 25;
   int x = (260 - 50)/2;
   wo_t *btn_wo = create_button(x, y, 50, 20, "Ok");
   button_t *btn = (button_t *)btn_wo->data;
   btn->release_func = (void*)&dialog_close;
   ui_add(msg->ui, btn_wo);

   ui_draw(msg->ui);

   if(title)
      dialog_set_title(msg, title);
   else
      dialog_set_title(msg, "Message");

   return true;
}

void dialog_complete(wo_t *wo, int window) {
   (void)wo;
   // find dialog
   dialog_t *dialog = dialog_from_window(window);
   if(!dialog) {
      debug_println("Couldn't find dialog");
      end_subroutine();
   }
   if(!dialog->active) {
      debug_println("Dialog not active\n");
      end_subroutine();
   }
   char *text = NULL;
   if(dialog->input_wo) {
      input_t *input = (input_t *)dialog->input_wo->data;
      text = input->text;
   }
   if(dialog->callback)
      dialog->callback(text, window);

   dialog_close(wo, window);
}

int dialog_yesno(char *title, char *text, void *return_func) {
   if(dialog_count == MAX_DIALOGS) {
      debug_println("Can't create dialog\n");
      return -1;
   }
   int index = get_free_dialog();
   if(index < 0) return false;
   dialog_t *dialog = dialogs[index];
   dialog_init(dialog, create_window(260, 100));
   dialog->callback = return_func;
   dialog->type = DIALOG_YESNO;
   dialog->input_wo = NULL;

   dialog_set_title(dialog, title);

   int y = 10;
   
   // dialog text

   wo_t *msg = create_label(15, y, 260 - 30, 20, text);
   ui_add(dialog->ui, msg);
   
   // buttons
   y += 25;
   int btnswidth = 50*2 + 10;
   int btnsx = (260 - btnswidth)/2;

   int x = btnsx;
   wo_t *btn_wo = create_button(x, y, 50, 20, "Yes");
   button_t *btn = (button_t *)btn_wo->data;
   btn->release_func = (void *)&dialog_complete;
   ui_add(dialog->ui, btn_wo);

   x += 50 + 10;

   wo_t *btn_cancel_wo = create_button(x, y, 50, 20, "No");
   button_t *btn_cancel = (button_t *)btn_cancel_wo->data;
   btn_cancel->release_func = (void*)&dialog_close;
   ui_add(dialog->ui, btn_cancel_wo);

   ui_draw(dialog->ui);

   return index;
}

int dialog_input(char *text, void *return_func) {
   if(dialog_count == MAX_DIALOGS) {
      debug_println("Can't create dialog\n");
      return -1;
   }
   int index = get_free_dialog();
   if(index < 0) return false;
   dialog_t *input = dialogs[index];
   dialog_init(input, create_window(260, 100));
   input->callback = return_func;
   input->type = DIALOG_INPUT;

   dialog_set_title(input, "Input Dialog");

   int y = 10;
   
   // dialog text

   wo_t *msg = create_label(15, y, 260 - 30, 20, text);
   ui_add(input->ui, msg);
   
   // text input
   y += 25;

   input->input_wo = create_input((260-160)/2, y, 160, 20);
   input_t *inputbox = (input_t *)input->input_wo->data;
   inputbox->return_func = (void *)&dialog_complete;
   inputbox->valign = true;
   inputbox->halign = true;
   input->input_wo->selected = true;
   input->ui->focused = input->input_wo;
   ui_add(input->ui, input->input_wo);

   // buttons
   y += 25;
   int btnswidth = 50*2 + 10;
   int btnsx = (260 - btnswidth)/2;

   int x = btnsx;
   wo_t *btn_wo = create_button(x, y, 50, 20, "Ok");
   button_t *btn = (button_t *)btn_wo->data;
   btn->release_func = (void *)&dialog_complete;
   ui_add(input->ui, btn_wo);

   x += 50 + 10;

   wo_t *btn_cancel_wo = create_button(x, y, 50, 20, "Cancel");
   button_t *btn_cancel = (button_t *)btn_cancel_wo->data;
   btn_cancel->release_func = (void*)&dialog_close;
   ui_add(input->ui, btn_cancel_wo);

   ui_draw(input->ui);

   return index;
}

// colourpicker dialog specific funcs

void dialog_colourpicker_update(uint16_t colour, dialog_t *dialog) {
   uint16_t *fb = (uint16_t*)dialog->surface.buffer;

   // calculate colour
   char buffer[9];
   sprintf(buffer, "0x%h", colour);
   set_input_text(dialog->input_wo, buffer);

   // set r,g,b inputs
   inttostr(get_r16(colour), buffer);
   wo_t *input = dialog_get(dialog, "red_input");
   set_input_text(input, buffer);
   inttostr(get_g16(colour), buffer);
   input = dialog_get(dialog, "green_input");
   set_input_text(input, buffer);
   inttostr(get_b16(colour), buffer);
   input = dialog_get(dialog, "blue_input");
   set_input_text(input, buffer);

   ui_draw(dialog->ui);

   // draw colour square
   for(int x = 0; x < 50; x++) {
      for(int y = 0; y < 50; y++) {
         fb[(y + 60) * dialog->surface.width + (x + 265)] = colour;
      }
   }
}

void dialog_colourpicker_click(int x, int y, int window) {
   dialog_t *dialog = dialog_from_window(window);
   if(!dialog) {
      debug_println("Couldn't find dialog for window %i\n", window);
      end_subroutine();
   }
   if(x >= 5 && x < 261 && y >= 60 && y < 366) {
      // clicked in colour picker area
      x -= 5; // offset
      y -= 60; // offset
      if(x < 0 || x >= 256 || y < 0 || y >= 256) {
         end_subroutine(); // out of bounds
         return;
      }
      uint16_t *fb = (uint16_t*)dialog->surface.buffer;
      uint16_t colour = fb[(y+60)*dialog->surface.width + (x+5)];
      dialog_colourpicker_update(colour, dialog);
   } else {
      ui_click(dialog->ui, x, y);
   }
   end_subroutine();
}

void dialog_colourpicker_drag(int x, int y, int window) {
   dialog_t *dialog = dialog_from_window(window);
   if(!dialog) {
      debug_println("Couldn't find dialog for window %i\n", window);
      end_subroutine();
   }
   if(x >= 5 && x < 261 && y >= 60 && y < 366) {
      // dragged in colour picker area
      x -= 5; // offset
      y -= 60; // offset
      if(x < 0 || x >= 256 || y < 0 || y >= 256) return; // out of bounds
      uint16_t *fb = (uint16_t*)dialog->surface.buffer;
      uint16_t colour = fb[(y+60)*dialog->surface.width + (x+5)];
      dialog_colourpicker_update(colour, dialog);
   } else {
      // no ui drag func
   }
   end_subroutine();
}

void dialog_colourpicker_input_return(wo_t *wo, int window) {
   input_t *input_data = wo->data;
   uint16_t colour = hextouint(input_data->text+2);
   dialog_t *dialog = dialog_from_window(window);
   if(!dialog) {
      debug_println("Couldn't find dialog for window %i\n", window);
      return;
   }
   dialog_colourpicker_update(colour, dialog);
}

void dialog_colourpicker_draw(dialog_t *dialog) {
   input_t *input = dialog->input_wo->data;
   uint16_t colour = hextouint(input->text+2);

   draw_context_t context = ui_get_context(dialog->ui);
   // draw colour square border
   draw_unfilledrect(&context, rgb16(0, 0, 0), 264, 59, 52, 52); // border
   // draw colour square
   for(int x = 0; x < 50; x++) {
      for(int y = 0; y < 50; y++) {
         draw_pixel(&context, colour, x + 265, y + 60);
      }
   }
   // draw the colour picker at (5, 50)
   draw_unfilledrect(&context, rgb16(0, 0, 0), 4, 59, 258, 258); // border
   int xoffset = 5;
   int yoffset = 60;
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
         
         draw_pixel(&context, colour, x + xoffset, y + yoffset);
      }
   }
}

void dialog_colourpicker_resize(uint32_t fb, int width, int height, int window) {
   (void)fb;
   (void)width;
   (void)height;
   dialog_t *dialog = dialog_from_window(window);
   if(!dialog) {
      debug_println("Couldn't find dialog for window %i\n", window);
      end_subroutine();
   }
   clear_w(window);
   dialog->surface = get_surface_w(window);
   ui_draw(dialog->ui);
   dialog_colourpicker_draw(dialog);
   end_subroutine();
}

void dialog_colourpicker_rgbinput_return(wo_t *input, int window) {
   (void)input;
   dialog_t *dialog = dialog_from_window(window);
   wo_t *input_r = dialog_get(dialog, "red_input");
   wo_t *input_g = dialog_get(dialog, "green_input");
   wo_t *input_b = dialog_get(dialog, "blue_input");
   int r = strtoint(get_input(input_r)->text);
   int g = strtoint(get_input(input_g)->text);
   int b = strtoint(get_input(input_b)->text);
   uint16_t colour = rgb16(r, g, b);
   dialog_colourpicker_update(colour, dialog);
}

int dialog_colourpicker(uint16_t colour, void (*return_func)(char *out, int window)) {
   int index = get_free_dialog();
   if(index < 0) return false;
   dialog_t *dialog = dialogs[index];
   dialog_init(dialog, create_window(320, 340));
   dialog->callback = return_func;
   dialog->type = DIALOG_COLOURPICKER;

   dialog_set_title(dialog, "Colour Picker");

   int x = (320-165)/2;

   wo_t *label = create_label(x, 8, 60, 20, "Colour: ");
   ((label_t*)label->data)->bordered = false;
   ui_add(dialog->ui, label);

   dialog->input_wo = create_input(x+65, 8, 100, 20);
   input_t *input_data = dialog->input_wo->data;
   input_data->valign = true;
   input_data->halign = true;
   input_data->return_func = &dialog_colourpicker_input_return;
   char buffer[8];
   strcpy(buffer, "0x");
   uinttohexstr(colour, buffer+2);
   set_input_text(dialog->input_wo, buffer);
   ui_add(dialog->ui, dialog->input_wo);

   wo_t *okbtn = create_button(x, 30, 80, 20, "Ok");
   set_button_release(okbtn, &dialog_complete);
   ui_add(dialog->ui, okbtn);

   wo_t *cancelbtn = create_button(x+85, 30, 80, 20, "Cancel");
   set_button_release(cancelbtn, &dialog_close);
   ui_add(dialog->ui, cancelbtn);

   wo_t *r_label = create_label(264, 140, 52, 16, "Red:");
   get_label(r_label)->valign = true;
   wo_t *r_input = create_input(264, 158, 52, 16);
   get_input(r_input)->valign = true;
   inttostr(get_r16(colour), buffer);
   set_input_text(r_input, buffer);
   set_input_return(r_input, &dialog_colourpicker_rgbinput_return);
   ui_add(dialog->ui, r_label);
   ui_add(dialog->ui, r_input);
   dialog_add(dialog, "red_input", r_input);
   wo_t *g_label = create_label(264, 174, 52, 16, "Green:");
   get_label(g_label)->valign = true;
   wo_t *g_input = create_input(264, 192, 52, 16);
   get_input(g_input)->valign = true;
   inttostr(get_g16(colour), buffer);
   set_input_text(g_input, buffer);
   set_input_return(g_input, &dialog_colourpicker_rgbinput_return);
   dialog_add(dialog, "green_input", g_input);
   ui_add(dialog->ui, g_label);
   ui_add(dialog->ui, g_input);
   wo_t *b_label = create_label(264, 210, 52, 16, "Blue:");
   get_label(b_label)->valign = true;
   wo_t *b_input = create_input(264, 228, 52, 16);
   get_input(b_input)->valign = true;
   inttostr(get_b16(colour), buffer);
   set_input_text(b_input, buffer);
   set_input_return(b_input, &dialog_colourpicker_rgbinput_return);
   dialog_add(dialog, "blue_input", b_input);
   ui_add(dialog->ui, b_label);
   ui_add(dialog->ui, b_input);

   ui_draw(dialog->ui);

   dialog_colourpicker_draw(dialog);

   override_click((uint32_t)&dialog_colourpicker_click, dialog->window);
   override_drag((uint32_t)&dialog_colourpicker_drag, dialog->window);
   override_resize((uint32_t)&dialog_colourpicker_resize, dialog->window);

   return index;
}

// filepicker specific

int dialog_filepicker_sort(const void *v1, const void *v2) {
   const fs_dir_entry_t *d1 = (const fs_dir_entry_t*)v1;
   const fs_dir_entry_t *d2 = (const fs_dir_entry_t*)v2;

   return strcmp(d1->filename, d2->filename);
}

void dialog_filepicker_show_dir(dialog_t *dialog);

int dialog_filepicker_grid_click(wo_t *grid, int window, int row, int col) {
   dialog_t *dialog = dialog_from_window(window);
   if(!dialog) {
      debug_println("Couldn't find dialog for window %i\n", window);
      return 0;
   }
   if(!dialog->dir) return 0;
   grid_t *grid_data = grid->data;
   grid_cell_t *cell = &grid_data->cells[row][col];
   if(cell->child_count > 0) {
      wo_t *label = cell->children[0];
      label_t *label_data = label->data;
      char *filename = label_data->label;
      char filepath[256];
      wo_t *input = dialog->ui->wos[1];
      input_t *input_data = input->data;
      strcpy(filepath, input_data->text);
      if(strlen(filepath) > 1)
         strcat(filepath, "/");
      strcat(filepath, filename);
      debug_println("Full path: %s", filepath);

      // find dir entry
      for(int i = 0; i < dialog->dir->size; i++) {
         fs_dir_entry_t *entry = &dialog->dir->entries[i];
         if(!strequ(entry->filename, filename)) continue;
         if(entry->type == FS_TYPE_DIR) {
            // show dir contents
            set_input_text(input, filepath);
            clear_w(dialog->window);
            dialog_filepicker_show_dir(dialog);
            ui_draw(dialog->ui);
            redraw_w(dialog->window);
            return 1;
         } else {
            // call return func
            if(dialog->callback)
               dialog->callback(filepath, window);
            dialog_close(NULL, dialog->window);
         }
         break;
      }
   }
   return 0;
}

void dialog_filepicker_show_dir(dialog_t *dialog) {
   ui_mgr_t *ui = dialog->ui;
   wo_t *input = ui->wos[1];
   char *path = ((input_t*)input->data)->text;

   // delete existing grid
   groupbox_t *groupbox = ui->wos[4]->data;
   canvas_t *canvas = (groupbox->canvas)->data;
   if(canvas->child_count > 0) {
      if(ui->hovered == canvas->children[0])
         ui->hovered = NULL;
      destroy_wo(canvas->children[0]);
      canvas->children[0] = NULL;
      canvas->child_count = 0;
   }

   if(dialog->dir) {
      free(dialog->dir->entries, dialog->dir->size * sizeof(fs_dir_entry_t));
      free(dialog->dir, sizeof(dialog->dir));
      dialog->dir = NULL;
   }
   
   fs_dir_content_t *content = read_dir(path);
   if(!content) {
      dialog_msg("Error", "Couldn't read directory");
      return;
   }
   
   dialog->dir = content;

   int width = get_width_w(ui->window);

   int cols = (width - 20) / 100;
   if(!cols) cols = 1;

   int rows = ((content->size - 2) + cols - 1) / cols;

   int grid_height = rows * 32;

   int box_height = grid_height + 20;
   if(box_height < 200)
      box_height = 200;

   dialog->content_height = box_height + ui->wos[4]->y + 10;
   int newwidth = set_content_height(dialog->content_height, dialog->window);

   if(newwidth != width) {
      ui->wos[3]->x = newwidth - ui->wos[3]->width - 10; // reposition cancel btn
      ui->wos[4]->width = newwidth - 20; // resize groupbox
      groupbox->canvas->width = ui->wos[4]->width - 2;
      width = newwidth;
   }

   // resize groupbox and canvas
   ui->wos[4]->height = box_height;
   groupbox->canvas->height = grid_height;

   wo_t *grid = create_grid(10, 5, groupbox->canvas->width - 20, grid_height, rows, cols);
   grid_t *grid_data = grid->data;
   grid_data->bordered = false;
   grid_data->click_func = &dialog_filepicker_grid_click;
   canvas_add(groupbox->canvas, grid);

   if(content->entries)
      sort(content->entries, content->size, sizeof(fs_dir_entry_t), dialog_filepicker_sort);

   int row = 0;
   int col = 0;

   int offset = strcmp((path), "/") == 0 ? 0 : 2; // hide dot entries
   for(int i = offset; i < content->size; i++) {
      fs_dir_entry_t *entry = &content->entries[i];
      if(entry->hidden)
         continue;

      // create label
      wo_t *label = create_label(24, 6, 76, 20, entry->filename);
      label_t *label_data = label->data;
      label_data->valign = true;
      label_data->halign = false;
      label_data->bordered = false;
      grid_add(grid, label, row, col);

      // icon
      wo_t *icon = create_image(0, 6, 20, 20, NULL);
      image_t *icon_data = icon->data;
      if(entry->type == FS_TYPE_DIR)
         icon_data->data = dialog->folder_icon_data;
      else
         icon_data->data = dialog->file_icon_data;
      
      grid_add(grid, icon, row, col);

      row++;
      if(row == rows) {
         row = 0;
         col++;
      }
   }
}

void dialog_filepicker_show_parent(wo_t *wo, int window) {
   (void)wo;
   dialog_t *dialog = dialog_from_window(window);
   if(!dialog) {
      debug_println("Couldn't find dialog for window %i\n", window);
      return;
   }
   scroll_to(0, dialog->window);
   input_t *input_data = dialog->ui->wos[1]->data;
   char *text = input_data->text;
   strsplit_last(text, NULL, text, '/');
   if(strlen(text) == 0) {
      strcpy(text, "/");
   }
   input_data->cursor_pos = strlen(text);
   clear_w(dialog->window);
   dialog_filepicker_show_dir(dialog);
   ui_draw(dialog->ui);
   redraw_w(dialog->window);
}

void dialog_filepicker_input_return(wo_t *wo, int window) {
   dialog_t *dialog = dialog_from_window(window);
   if(!dialog) {
      debug_println("Couldn't find dialog for window %i\n", window);
      return;
   }
   input_t *input_data = wo->data;
   char *text = input_data->text;
   if(strlen(text) == 0) {
      strcpy(text, "/");
      input_data->cursor_pos = 1;
   } else if(strendswith(text, "/") && strlen(text) > 1) {
      text[strlen(text)-1] = '\0';
      input_data->cursor_pos--;
   }
   clear_w(dialog->window);
   dialog_filepicker_show_dir(dialog);
   ui_draw(dialog->ui);
}

void dialog_filepicker_scroll(int deltaY, int offsetY, int window) {
   dialog_t *dialog = dialog_from_window(window);
   if(!dialog) {
      debug_println("Couldn't find dialog for window %i\n", window);
      return;
   }
   ui_scroll(dialog->ui, deltaY, offsetY);
   //clear_w(ui->window);
   ui_draw(dialog->ui);
   redraw_w(dialog->window);
   
   end_subroutine();
}

void dialog_filepicker_resize(uint32_t fb, int width, int height, int window) {
   (void)fb;
   (void)height;
   dialog_t *dialog = dialog_from_window(window);
   if(!dialog) {
      debug_println("Couldn't find dialog for window %i\n", window);
      return;
   }
   scroll_to(0, window);
   dialog->surface = get_surface_w(window);
   dialog->ui->surface = &dialog->surface;
   dialog->ui->wos[4]->width = width - 20; // resize groupbox
   dialog->ui->wos[3]->x = width - dialog->ui->wos[3]->width - 10; // reposition cancel btn
   groupbox_t *groupbox = dialog->ui->wos[4]->data;
   groupbox->canvas->width = dialog->ui->wos[4]->width - 2;
   dialog_filepicker_show_dir(dialog);
   clear_w(dialog->window);
   ui_draw(dialog->ui);
   end_subroutine();
}

uint16_t *dialog_load_icon(char *path, int *width, int *height) {
   // load 16 bit bmp
   FILE *f = fopen(path, "r");
   if(!f)
      return false;
   int size = fsize(fileno(f));
   uint16_t *icon = malloc(size);
   fread(icon, size, 1, f);
   fclose(f);
   bmp_header_t *icon_bmp = (bmp_header_t*)icon;
   bmp_info_t *icon_info = (bmp_info_t*)((uint8_t*)icon+sizeof(bmp_header_t));
   icon += icon_bmp->dataOffset/sizeof(uint16_t);
   int w = icon_info->width;
   int h = icon_info->height;
   // flip data upside down
   for(int y = 0; y < h / 2; y++) {
      for(int x = 0; x < w; x++) {
         uint16_t temp = icon[y * w + x];
         icon[y * w + x] = icon[(h - 1 - y) * w + x];
         icon[(h - 1 - y) * w + x] = temp;
      }
   }
   if(width)
      *width = w;
   if(height)
      *height = h;
   return icon;
}

int dialog_filepicker(char *startpath, void (*return_func)(char *out, int window)) {
   int index = get_free_dialog();
   if(index < 0) return false;
   dialog_t *dialog = dialogs[index];
   int width = 335;
   int height = 280;
   dialog_init(dialog, create_window(width, height));
   dialog->callback = return_func;
   dialog->type = DIALOG_FILEPICKER;
   dialog->dir = NULL;

   dialog_set_title(dialog, "File Picker");

   dialog->surface = get_surface_w(dialog->window);
   dialog->ui = ui_init(&dialog->surface, dialog->window);

   wo_t *label = create_label(5, 5, 60, 20, "Path: ");
   label_t *label_data = label->data;
   label_data->bordered = false;
   ui_add(dialog->ui, label);

   wo_t *input = create_input(70, 5, 100, 20);
   set_input_text(input, startpath);
   input_t *input_data = input->data;
   input_data->valign = true;
   input_data->return_func = &dialog_filepicker_input_return;
   ui_add(dialog->ui, input);

   wo_t *upbtn = create_button(170, 5, 35, 20, "Up");
   set_button_release(upbtn, &dialog_filepicker_show_parent);
   ui_add(dialog->ui, upbtn);

   wo_t *cancelbtn = create_button(width - 60, 5, 50, 20, "Cancel");
   set_button_release(cancelbtn, &dialog_close);
   ui_add(dialog->ui, cancelbtn);

   wo_t *groupbox = create_groupbox(10, 30, width - 20, 200, "Files");
   ui_add(dialog->ui, groupbox);

   // load 20x20 icons
   dialog->file_icon_data = dialog_load_icon("/bmp/file20.bmp", NULL, NULL);
   dialog->folder_icon_data = dialog_load_icon("/bmp/folder20.bmp", NULL, NULL);

   create_scrollbar(&dialog_filepicker_scroll, dialog->window);
   override_resize((uint32_t)&dialog_filepicker_resize, dialog->window);

   dialog_filepicker_show_dir(dialog);
   ui_draw(dialog->ui);

   return index;
}

// custom 'colourbox' wo which launches colourpicker

void dialog_colourbox_callback(char *out, int window) {
   dialog_t *dialog = dialog_from_window(window);
   if(!dialog) {
      debug_println("Couldn't find dialog for window %i\n", window);
      return;
   }
   dialog_wo_t *colourbox = (dialog_wo_t *)dialog->state;
   dialog_t *colourbox_dialog = dialog_from_window(colourbox->window);
   if(!colourbox_dialog) {
      debug_println("Couldn't find dialog for window %i\n", colourbox->window);
      return;
   }
   uint16_t colour = hextouint(out + 2);
   wo_t *label = &colourbox->wo;
   label_t *label_data = label->data;
   label_data->colour_bg = colour;
   ui_draw(colourbox_dialog->ui);
   colourbox->callback(out, colourbox->window, (wo_t*)&colourbox->wo);
}

void dialog_colourbox_release(wo_t *wo, int window) {
   dialog_t *dialog = dialog_from_window(window);
   if(!dialog) {
      debug_println("Couldn't find dialog for window %i\n", window);
      return;
   }
   label_t *label_data = wo->data;
   int d = dialog_colourpicker(label_data->colour_bg, &dialog_colourbox_callback);
   get_dialog(d)->state = wo;
}

wo_t *dialog_create_colourbox(int x, int y, int width, int height, uint16_t colour, int window, void (*callback)(char *out, int window, wo_t *colourbox)) {
   dialog_wo_t *colourbox = (dialog_wo_t*)create_label(x, y, width, height, "");
   colourbox->callback = callback;
   colourbox->window = window;
   // ideally should resize, for now malloc gives enough space
   wo_t *label = &colourbox->wo;
   label_t *label_data = label->data;
   label_data->colour_bg = colour;
   label_data->filled = true;
   label_data->release_func = &dialog_colourbox_release;
   return label;
}

// custom 'browse for file' button

void dialog_browsebtn_callback(char *out, int window) {
   dialog_t *dialog = dialog_from_window(window);
   if(!dialog) {
      debug_println("Couldn't find dialog for window %i\n", window);
      return;
   }
   dialog_wo_t *browsebtn = (dialog_wo_t *)dialog->state;
   dialog_t *browsebtn_dialog = dialog_from_window(browsebtn->window);
   if(!browsebtn_dialog) {
      debug_println("Couldn't find dialog for window %i\n", browsebtn->window);
      return;
   }
   browsebtn->callback(out, browsebtn->window, (wo_t*)&browsebtn->wo);
}

void dialog_browsebtn_release(wo_t *wo, int window) {
   dialog_t *dialog = dialog_from_window(window);
   if(!dialog) {
      debug_println("Couldn't find dialog for window %i\n", window);
      return;
   }
   dialog_wo_t *browsebtn = (dialog_wo_t *)wo;
   char *startdir = (char*)browsebtn->state;
   int d = dialog_filepicker(startdir, &dialog_browsebtn_callback);
   get_dialog(d)->state = wo;
}

wo_t *dialog_create_browsebtn(int x, int y, int width, int height, int window, char *text, char *startpath, void (*callback)(char *out, int window, wo_t *browsebtn)) {
   dialog_wo_t *browsebtn = (dialog_wo_t*)create_button(x, y, width, height, text);
   browsebtn->window = window;
   wo_t *btn = &browsebtn->wo;
   set_button_release(btn, &dialog_browsebtn_release);
   browsebtn->callback = callback;
   browsebtn->state = startpath;
   return &browsebtn->wo;
}

// window settings dialog

void dialog_window_set_bgcolour(char *out, int window, wo_t *colourbox) {
   (void)colourbox;
   dialog_t *dialog = dialog_from_window(window);
   uint16_t colour = hextouint(out+2);
   set_window_setting(W_SETTING_BGCOLOUR, colour, dialog->parentWindow);
   set_input_text(dialog_get(dialog, "bgcolour_input"), out);
   ui_draw(dialog->ui);
   dialog_t *parentdialog = dialog_from_window(dialog->parentWindow);
   if(!parentdialog) return;
   clear_w(dialog->parentWindow);
   ui_draw(parentdialog->ui);
   redraw_w(dialog->parentWindow);
}

void dialog_window_set_txtcolour(char *out, int window, wo_t *colourbox) {
   (void)colourbox;
   dialog_t *dialog = dialog_from_window(window);
   uint16_t colour = hextouint(out+2);
   set_window_setting(W_SETTING_TXTCOLOUR, colour, dialog->parentWindow);
   set_input_text(dialog_get(dialog, "txtcolour_input"), out);
   ui_draw(dialog->ui);
   dialog_t *parentdialog = dialog_from_window(dialog->parentWindow);
   if(!parentdialog) return;
   clear_w(dialog->parentWindow);
   ui_draw(parentdialog->ui);
   redraw_w(dialog->parentWindow);
}

void dialog_window_set_bgcolour_input(wo_t *wo, int window) {
   dialog_t *dialog = dialog_from_window(window);
   input_t *input = wo->data;
   uint16_t colour = hextouint(input->text+2);
   set_window_setting(W_SETTING_BGCOLOUR, colour, dialog->parentWindow);
   label_t *colourbox = dialog_get(dialog, "bgcolour_colourbox")->data;
   colourbox->colour_bg = colour;
   ui_draw(dialog->ui);
   dialog_t *parentdialog = dialog_from_window(dialog->parentWindow);
   if(!parentdialog) return;
   clear_w(dialog->parentWindow);
   ui_draw(parentdialog->ui);
   redraw_w(dialog->parentWindow);
}

void dialog_window_set_txtcolour_input(wo_t *wo, int window) {
   dialog_t *dialog = dialog_from_window(window);
   input_t *input = wo->data;
   uint16_t colour = hextouint(input->text+2);
   set_window_setting(W_SETTING_TXTCOLOUR, colour, dialog->parentWindow);
   label_t *colourbox = dialog_get(dialog, "txtcolour_colourbox")->data;
   colourbox->colour_bg = colour;
   ui_draw(dialog->ui);
   dialog_t *parentdialog = dialog_from_window(dialog->parentWindow);
   if(!parentdialog) return;
   clear_w(dialog->parentWindow);
   ui_draw(parentdialog->ui);
   redraw_w(dialog->parentWindow);
}

int dialog_window_settings(int window, char *title) {
   int y = 10;
   int index = get_free_dialog();
   if(index < 0) return -1;
   dialog_t *dialog = dialogs[index];

   dialog_init(dialog, create_window(280, 100));
   dialog->type = DIALOG_SETTINGS;
   dialog->parentWindow = window;

   char title_buffer[256];
   snprintf(title_buffer, 256, "Window settings for '%s'", title);

   wo_t *groupbox = create_groupbox(15, y, 280 - 30, 65, title_buffer);
   char buffer[8];
   strcpy(buffer, "0x");

   uint16_t colour = get_window_setting(W_SETTING_BGCOLOUR, window);
   wo_t *label = create_label(5, 5, 135, 20, "Background colour");
   wo_t *input = create_input(145, 5, 80, 20);
   input_t *input_data = input->data;
   input_data->valign = true;
   input_data->halign = true;
   input_data->return_func = &dialog_window_set_bgcolour_input;
   uinttohexstr(colour, buffer+2);
   set_input_text(input, buffer);
   wo_t *colourbox = dialog_create_colourbox(225, 5, 20, 20, colour, dialog->window, &dialog_window_set_bgcolour);
   dialog_add(dialog, "bgcolour_colourbox", colourbox);
   groupbox_add(groupbox, label);
   groupbox_add(groupbox, input);
   groupbox_add(groupbox, colourbox);
   dialog_add(dialog, "bgcolour_input", input);

   colour = get_window_setting(W_SETTING_TXTCOLOUR, window);
   label = create_label(5, 30, 135, 20, "Text colour");
   input = create_input(145, 30, 80, 20);
   input_data = input->data;
   input_data->valign = true;
   input_data->halign = true;
   input_data->return_func = &dialog_window_set_txtcolour_input;
   uinttohexstr(colour, buffer+2);
   set_input_text(input, buffer);
   colourbox = dialog_create_colourbox(225, 30, 20, 20, 0, dialog->window, &dialog_window_set_txtcolour);
   dialog_add(dialog, "txtcolour_colourbox", colourbox);
   groupbox_add(groupbox, label);
   groupbox_add(groupbox, input);
   groupbox_add(groupbox, colourbox);
   dialog_add(dialog, "txtcolour_input", input);

   ui_add(dialog->ui, groupbox);
   ui_draw(dialog->ui);

   dialog_set_title(dialog, "Window Settings");

   return index;
}

void dialog_init_overrides(int window) {
   override_click((uint32_t)&dialog_click, window);
   override_hover((uint32_t)&dialog_hover, window);
   override_release((uint32_t)&dialog_release, window);
   override_keypress((uint32_t)&dialog_keypress, window);
   override_close((uint32_t)&dialog_window_close, window);
   override_resize((uint32_t)&dialog_resize, window);
   override_rightclick((uint32_t)&dialog_rightclick, window);
   override_mouseout((uint32_t)&dialog_mouseout, window);
   override_draw(0, window);
}

dialog_t *get_dialog(int index) {
   if(index < 0 || index >= dialog_count)
      return NULL;
   return dialogs[index];
}