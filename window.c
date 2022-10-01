// default window behaviour

#include "window.h"

#include "gui.h"

extern int strlen(char* str);

// default terminal behaviour

void window_term_keypress(char key, int windowIndex) {
   if(((key >= 'A') && (key <= 'Z')) || ((key >= '0') && (key <= '9')) || (key == ' ')
   || (key == '/') || (key == '.')) {

      // write to current window
      gui_window_t *selected = &(gui_get_windows()[windowIndex]);
      if(selected->text_index < TEXT_BUFFER_LENGTH-1) {
         selected->text_buffer[selected->text_index] = key;
         selected->text_buffer[selected->text_index+1] = '\0';
         selected->text_index++;
         selected->text_x = (selected->text_index)*(FONT_WIDTH+FONT_PADDING);
      }

      gui_window_draw(windowIndex);

   }
}

void window_term_return(void *regs, int windowIndex) {
   gui_window_t *selected = &(gui_get_windows()[windowIndex]);

   // write cmd to window framebuffer
   gui_window_drawcharat('>', 8, 1, selected->text_y, windowIndex);
   gui_window_writestrat(selected->text_buffer, 0, 1 + FONT_WIDTH + FONT_PADDING, selected->text_y, windowIndex);
   
   // write cmd to cmdbuffer
   // shift all entries up
   for(int i = CMD_HISTORY_LENGTH-1; i > 0; i--)
      strcpy(selected->cmd_history[i], selected->cmd_history[i-1]);
   // add new entry
   strcpy_fixed(selected->cmd_history[0], selected->text_buffer, selected->text_index);
   selected->cmd_history[0][selected->text_index] = '\0';
   selected->cmd_history_pos = -1;

   gui_window_drawchar('\n', 0, windowIndex);
   
   gui_checkcmd(regs);
   selected = &(gui_get_windows()[windowIndex]); // in case the buffer has changed during the cmd 

   selected->text_index = 0;
   selected->text_buffer[selected->text_index] = '\0';
   selected->needs_redraw = true;

   gui_window_draw(windowIndex);
}

void window_term_backspace(int windowIndex) {
   gui_window_t *selected = &(gui_get_windows()[windowIndex]);
   if(selected->text_index > 0) {
      selected->text_index--;
      selected->text_x-=FONT_WIDTH+FONT_PADDING;
      selected->text_buffer[selected->text_index] = '\0';
   }

   gui_window_draw(windowIndex);
}

void window_term_uparrow(int windowIndex) {
   gui_window_t *selected = &(gui_get_windows()[windowIndex]);

   selected->cmd_history_pos++;
   if(selected->cmd_history_pos == CMD_HISTORY_LENGTH)
      selected->cmd_history_pos = CMD_HISTORY_LENGTH - 1;

   int len = strlen(selected->cmd_history[selected->cmd_history_pos]);
   strcpy_fixed(selected->text_buffer, selected->cmd_history[selected->cmd_history_pos], len);
   selected->text_index = len;
   selected->text_x=len*(FONT_WIDTH+FONT_PADDING);
   gui_window_draw(windowIndex);
}

void window_term_downarrow(int windowIndex) {
   gui_window_t *selected = &(gui_get_windows()[windowIndex]);

   selected->cmd_history_pos--;

   if(selected->cmd_history_pos <= -1) {
      selected->cmd_history_pos = -1;
      selected->text_index = 0;
      selected->text_x = FONT_PADDING;
      selected->text_buffer[0] = '\0';
   } else {
      int len = strlen(selected->cmd_history[selected->cmd_history_pos]);
      strcpy_fixed(selected->text_buffer, selected->cmd_history[selected->cmd_history_pos], len);
      selected->text_index = len;
      selected->text_x=len*(FONT_WIDTH+FONT_PADDING);
   }
   gui_window_draw(windowIndex);
}
