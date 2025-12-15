#include "dialogs.h"

#include "../prog.h"
#include "stdio.h"

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

void dialog_close(void *wo, int window) {
   (void)wo;
   if(!dialog_from_window(window)) {
      debug_println("Can't find window %i", window);
      end_subroutine();
      return;
   }
   dialog_from_window(window)->active = false;
   close_window(window);
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

void dialog_complete(void *wo, int window) {
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
      dialog->callback(text);

   dialog_close(wo, window);

   end_subroutine();
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

dialog_t *get_dialog(int index) {
   if(index < 0 || index >= dialog_count)
      return NULL;
   return dialogs[index];
}