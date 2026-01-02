#include "dialogs.h"

#include "../prog.h"
#include "stdio.h"
#include "draw.h"
#include "../../lib/string.h"

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
   if(!dialog_from_window(window)) {
      debug_println("Can't find window %i", window);
      end_subroutine();
      return;
   }
   dialog_from_window(window)->active = false;
   close_window(window);
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

   end_subroutine();
}

bool dialog_msg(char *title, char *text) {
   int y = 10;
   int index = get_free_dialog();
   if(index < 0) return false;
   dialog_t *msg = dialogs[index];

   msg->window = create_window(260, 100);
   msg->active = true;
   msg->type = DIALOG_MSG;

   // create ui mgr
   msg->surface = get_surface_w(msg->window);
   msg->ui = ui_init(&msg->surface, msg->window);

   override_click((uint32_t)&dialog_click, msg->window);
   override_release((uint32_t)&dialog_release, msg->window);
   override_hover((uint32_t)&dialog_hover, msg->window);
   override_keypress((uint32_t)&dialog_keypress, msg->window);

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
      set_window_title_w(msg->window, title);
   else
      set_window_title_w(msg->window, "Message");

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
   input_t *input = (input_t *)dialog->input_wo->data;
   char *text = input->text;
   if(dialog->callback)
      dialog->callback(text, window);

   dialog_close(wo, window);
}

int dialog_input(char *text, void *return_func) {
   if(dialog_count == MAX_DIALOGS) {
      debug_println("Can't create dialog\n");
      return -1;
   }
   int index = get_free_dialog();
   if(index < 0) return false;
   dialog_t *input = dialogs[index];
   input->window = create_window(260, 100);
   input->callback = return_func;
   input->active = true;
   input->type = DIALOG_INPUT;

   set_window_title_w(input->window, "Input Dialog");

   // create ui mgr
   input->surface = get_surface_w(input->window);
   input->ui = ui_init(&input->surface, input->window);

   override_click((uint32_t)&dialog_click, input->window);
   override_release((uint32_t)&dialog_release, input->window);
   override_hover((uint32_t)&dialog_hover, input->window);
   override_keypress((uint32_t)&dialog_keypress, input->window);

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

void dialog_colourpicker_update(int x, int y, dialog_t *dialog) {
   uint16_t *fb = (uint16_t*)dialog->surface.buffer;

   // calculate colour
   uint16_t colour = fb[(y+60)*dialog->surface.width + (x+5)];
   char buffer[9];
   sprintf(buffer, "0x%h", colour);
   set_input_text(dialog->input_wo, buffer);
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
      if(x < 0 || x >= 256 || y < 0 || y >= 256) return; // out of bounds

      uint16_t *fb = (uint16_t*)dialog->surface.buffer;
      dialog_colourpicker_update(x, y, dialog);
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
      dialog_colourpicker_update(x, y, dialog);
   } else {
      // no ui drag func
   }
   end_subroutine();
}

void dialog_colourpicker_release(int x, int y, int window) {
   dialog_t *dialog = dialog_from_window(window);
   if(!dialog) {
      debug_println("Couldn't find dialog for window %i\n", window);
      end_subroutine();
   }
   ui_release(dialog->ui, x, y);
   end_subroutine();
}

void dialog_colourpicker_hover(int x, int y, int window) {
   dialog_t *dialog = dialog_from_window(window);
   if(!dialog) {
      debug_println("Couldn't find dialog for window %i\n", window);
      end_subroutine();
   }
   ui_hover(dialog->ui, x, y);
   end_subroutine();
}

void dialog_colourpicker_keypress(int c, int window) {
   dialog_t *dialog = dialog_from_window(window);
   if(!dialog) {
      debug_println("Couldn't find dialog for window %i\n", window);
      end_subroutine();
   }
   ui_keypress(dialog->ui, (uint16_t)c);
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
   // draw colour square
   uint16_t *fb = (uint16_t*)dialog->surface.buffer;
   for(int x = 0; x < 50; x++) {
      for(int y = 0; y < 50; y++) {
         fb[(y + 60) * dialog->surface.width + (x + 265)] = colour;
      }
   }
}

int dialog_colourpicker(uint16_t colour, void *return_func) {
   int index = get_free_dialog();
   if(index < 0) return false;
   dialog_t *dialog = dialogs[index];
   dialog->window = create_window(320, 340);
   dialog->callback = return_func;
   dialog->active = true;
   dialog->type = DIALOG_COLOURPICKER;

   set_window_title_w(dialog->window, "Colour Picker");

   dialog->surface = get_surface_w(dialog->window);
   dialog->ui = ui_init(&dialog->surface, dialog->window);

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

   wo_t *okbtn = create_button(x, 30, 80, 23, "Ok");
   set_button_release(okbtn, &dialog_complete);
   ui_add(dialog->ui, okbtn);

   wo_t *cancelbtn = create_button(x+85, 30, 80, 23, "Cancel");
   set_button_release(cancelbtn, &dialog_close);
   ui_add(dialog->ui, cancelbtn);

   ui_draw(dialog->ui);

   // draw
   uint16_t *fb = (uint16_t*)dialog->surface.buffer;

   // draw colour square border
   draw_unfilledrect(&dialog->surface, rgb16(0, 0, 0), 264, 59, 52, 52); // border
   // draw colour square
   for(int x = 0; x < 50; x++) {
      for(int y = 0; y < 50; y++) {
         fb[(y + 60) * dialog->surface.width + (x + 265)] = colour;
      }
   }
   // draw the colour picker at (5, 50)
   draw_unfilledrect(&dialog->surface, rgb16(0, 0, 0), 4, 59, 258, 258); // border
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
         
         fb[(y+yoffset)*dialog->surface.width + x+xoffset] = colour;
      }
   }

   override_click((uint32_t)&dialog_colourpicker_click, dialog->window);
   override_hover((uint32_t)&dialog_colourpicker_hover, dialog->window);
   override_release((uint32_t)&dialog_colourpicker_release, dialog->window);
   override_drag((uint32_t)&dialog_colourpicker_drag, dialog->window);
   override_keypress((uint32_t)&dialog_colourpicker_keypress, dialog->window);

   return index;
}

dialog_t *get_dialog(int index) {
   if(index < 0 || index >= dialog_count)
      return NULL;
   return dialogs[index];
}