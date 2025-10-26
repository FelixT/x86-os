#include "dialogs.h"

#include "../prog.h"
#include "stdio.h"

int dialog_count = 0;
dialog_t *dialogs[MAX_DIALOGS];

dialog_t *dialog_from_window(int window) {
   for(int i = 0; i < dialog_count; i++) {
      if(dialogs[i]->window == window)
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

bool dialog_msg(char *title, char *text) {
   int y = 10;
   int index = get_free_dialog();
   if(index < 0) return false;
   dialog_t *msg = dialogs[index];

   msg->window = create_window(260, 100);
   msg->active = true;
   msg->type = DIALOG_MSG;

   windowobj_t *txt = create_text_static_w(msg->window, NULL, 15, y, text);
   txt->width = 260 - 30;
   txt->texthalign = true;

   y += 25;
   int x = (260 - 50)/2;
   windowobj_t *btn = create_button_w(msg->window, NULL, x, y, "Ok");
   btn->release_func = (void*)&dialog_close;
   btn->width = 50;

   if(title)
      set_window_title_w(msg->window, title);

   return true;
}

void dialog_complete(void *wo, int window) {
   (void)wo;
   // find dialog
   dialog_t *dialog = dialog_from_window(window);
   if(!dialog->active) {
      debug_println("Dialog not active\n");
      return;
   }
   char *text = dialog->inputtxt->text;
   if(dialog->callback)
      dialog->callback(text);

   dialog_close(wo, window);

   end_subroutine();
}

void dialog_cancel(void *wo, int window) {
   (void)wo;
   debug_println("Cancel window %i", window);
   // find dialog
   dialog_t *dialog = dialog_from_window(window);
   if(!dialog) {
      debug_println("Couldn't find dialog");
      end_subroutine();
   }
   char *text = dialog->inputtxt->text;
   debug_println("Dialog output %s", text);

   if(dialog->callback)
      dialog->callback(text);

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

   int y = 10;
   
   // dialog text
   windowobj_t *txt = create_text_static_w(input->window, NULL, 15, y, text);
   txt->width = 260 - 30;
   txt->texthalign = true;

   // text input
   y += 25;
   input->inputtxt = create_text_w(input->window, NULL, (260 - 160)/2, y, "");
   input->inputtxt->width = 160;
   input->inputtxt->clicked = true;
   input->inputtxt->oneline = true;
   input->inputtxt->return_func = (void*)&dialog_complete;

   // buttons
   y += 25;
   int btnswidth = 50*2 + 10;
   int btnsx = (260 - btnswidth)/2;

   int x = btnsx;
   windowobj_t *btn = create_button_w(input->window, NULL, x, y, "Ok");
   btn->release_func = (void*)&dialog_complete;
   btn->width = 50;

   x += 50 + 10;

   windowobj_t *btn_cancel = create_button_w(input->window, NULL, x, y, "Cancel");
   btn_cancel->width = 50;
   btn_cancel->release_func = (void*)&dialog_close;

   return index;
}