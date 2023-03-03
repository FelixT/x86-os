#include "gui.h"
#include "draw.h"

extern int videomode;

int gui_mouse_x = 0;
int gui_mouse_y = 0;

uint16_t gui_bg;

bool mouse_enabled = false;
bool mouse_held = false;

gui_window_t *gui_windows;
int gui_selected_window = 0;

surface_t surface;

uint16_t cursor_buffer[FONT_WIDTH*FONT_HEIGHT]; // store whats behind cursor so it can be restored

gui_window_t *gui_windows;
int NUM_WINDOWS;

bool desktop_enabled = false;
uint8_t *icon_window;
uint8_t *gui_bgimage;

static inline void set_framebuffer(int index, uint16_t colour) {
   if(index < 0 || index >= (int)surface.width*(int)surface.height) {
      //gui_window_writestr("Attempted to write outside framebuffer bounds\n", 0, 0);
   } else {
      ((uint16_t*)surface.buffer)[index] = colour;
   }
}

surface_t getsurfacefromwindow(int windowIndex) {
   gui_window_t *window = &gui_windows[windowIndex];
   surface_t surface;
   surface.width = window->width;
   surface.height = window->height;
   surface.buffer = (uint32_t)window->framebuffer;
   return surface;
}

uint16_t gui_rgb16(uint8_t r, uint8_t g, uint8_t b) {
   // 5r 6g 5b
   return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

void gui_drawrect(uint16_t colour, int x, int y, int width, int height) {
   draw_rect(&surface, colour, x, y, width, height);
}

void gui_drawunfilledrect(uint16_t colour, int x, int y, int width, int height) {
   draw_unfilledrect(&surface, colour, x, y, width, height);
}

void gui_drawdottedrect(uint16_t colour, int x, int y, int width, int height) {
   draw_dottedrect(&surface, colour, x, y, width, height);
}

void gui_drawline(uint16_t colour, int x, int y, bool vertical, int length) {
   draw_line(&surface, colour, x, y, vertical, length);
}

void gui_clear(uint16_t colour) {
   gui_drawrect(colour, 0, 0, surface.width, surface.height);
}

void gui_drawcharat(char c, uint16_t colour, int x, int y) {
   draw_char(&surface, c, colour, x, y);
}

void gui_window_drawcharat(char c, uint16_t colour, int x, int y, int windowIndex) {
   if(c >= 'a' && c <= 'z') c = (c - 'a') + 'A'; // convert to uppercase
   surface_t surface = getsurfacefromwindow(windowIndex);
   draw_char(&surface, c, colour, x, y);
}

void gui_window_drawrect(uint16_t colour, int x, int y, int width, int height, int windowIndex) {
   surface_t surface = getsurfacefromwindow(windowIndex);
   draw_rect(&surface, colour, x, y, width, height);
}

void gui_window_writestrat(char *c, uint16_t colour, int x, int y, int windowIndex) {
   int i = 0;
   while(c[i] != '\0') {
      gui_window_drawcharat(c[i++], colour, x, y, windowIndex);
      x+=FONT_WIDTH+FONT_PADDING;
   }
}

void gui_window_writenumat(int num, uint16_t colour, int x, int y, int windowIndex) {
   if(num < 0)
      gui_window_drawcharat('-', colour, x+=(FONT_WIDTH+FONT_PADDING), y, windowIndex);

   char out[20];
   inttostr(num, out);
   gui_window_writestrat(out, colour, x, y, windowIndex);
}

void gui_window_drawchar(char c, uint16_t colour, int windowIndex) {

   gui_window_t *selected = &gui_windows[windowIndex];

   if(c == '\n') {
      selected->text_x = FONT_PADDING;
      selected->text_y += FONT_HEIGHT + FONT_PADDING;

      if(selected->text_y > selected->height - (TITLEBAR_HEIGHT + (FONT_HEIGHT+FONT_PADDING))) {
         window_scroll(windowIndex);
      }

      // immediately output each line
      selected->needs_redraw=true;
      gui_window_draw(windowIndex);

      return;
   }

   // x overflow
   if(selected->text_x + FONT_WIDTH + FONT_PADDING >= selected->width) {
      gui_window_drawcharat('-', colour, selected->text_x-2, selected->text_y, windowIndex);
      selected->text_x = FONT_PADDING;
      selected->text_y += FONT_HEIGHT + FONT_PADDING;

      if(selected->text_y > selected->height - (TITLEBAR_HEIGHT + (FONT_HEIGHT+FONT_PADDING))) {
         window_scroll(windowIndex);
      }
   }

   gui_window_drawcharat(c, colour, selected->text_x, selected->text_y, windowIndex);
   selected->text_x+=FONT_WIDTH+FONT_PADDING;

   if(selected->text_y > selected->height - (TITLEBAR_HEIGHT + (FONT_HEIGHT+FONT_PADDING))) {
      window_scroll(windowIndex);
   }

   selected->needs_redraw = true;

}

void gui_window_writestr(char *c, uint16_t colour, int windowIndex) {
   int i = 0;
   while(c[i] != '\0')
      gui_window_drawchar(c[i++], colour, windowIndex);
}

void gui_window_writenum(int num, uint16_t colour, int windowIndex) {
   if(num < 0)
      gui_window_drawchar('-', colour, windowIndex);

   char out[20];
   inttostr(num, out);
   gui_window_writestr(out, colour, windowIndex);
}

void gui_window_writeuint(uint32_t num, uint16_t colour, int windowIndex) {
   char out[20];
   uinttostr(num, out);
   gui_window_writestr(out, colour, windowIndex);
}

void gui_window_clearbuffer(gui_window_t *window, uint16_t colour) {
   for(int i = 0; i < window->width*(window->height-TITLEBAR_HEIGHT); i++) {
      window->framebuffer[i] = colour;
   }
}

void gui_redrawall();
void gui_drawchar(char c, uint16_t colour) {

   if(gui_selected_window < 0)
      return;

   gui_window_drawchar(c, colour, gui_selected_window);

}

void gui_writestr(char *c, uint16_t colour) {
   int i = 0;
   while(c[i] != '\0')
      gui_drawchar(c[i++], colour);
}

void gui_writestrat(char *c, uint16_t colour, int x, int y) {
   int i = 0;
   while(c[i] != '\0') {
      gui_drawcharat(c[i++], colour, x, y);
      x+=FONT_WIDTH+FONT_PADDING;
   }
}

void gui_writenum(int num, uint16_t colour) {
   if(num < 0)
      gui_drawchar('-', colour);

   char out[20];
   inttostr(num, out);
   gui_writestr(out, colour);
}

void gui_writeuint_hex(uint32_t num, uint16_t colour) {
   char out[20];
   uinttohexstr(num, out);
   gui_writestr(out, colour);
}

void gui_writeuint(uint32_t num, uint16_t colour) {
   char out[20];
   uinttostr(num, out);
   gui_writestr(out, colour);
}

void gui_writenumat(int num, uint16_t colour, int x, int y) {
   char out[20];
   inttostr(num, out);
   gui_writestrat(out, colour, x, y);
}

void gui_writeuintat(uint32_t num, uint16_t colour, int x, int y) {
   char out[20];
   uinttostr(num, out);
   gui_writestrat(out, colour, x, y);
}

extern vbe_mode_info_t vbe_mode_info_structure;
void gui_init(void) {
   videomode = 1;

   gui_bg = COLOUR_CYAN;

   surface.width = vbe_mode_info_structure.width;
   surface.height = vbe_mode_info_structure.height;
   surface.buffer = vbe_mode_info_structure.framebuffer;

   // reserve framebuffer memory so malloc can't assign it
   memory_reserve(surface.buffer, (int)surface.width*(int)surface.height);
   
   gui_clear(gui_bg);

   // init with one terminal window
   NUM_WINDOWS = 1;
   gui_windows = malloc(sizeof(gui_window_t));
   window_init(&gui_windows[0]);
   gui_windows[0].title[0] = '0';
   gui_selected_window = 0;
}

void gui_desktop_draw();
void gui_window_draw(int windowIndex);
void gui_draw(void) {
   //gui_clear(3);   
   // make sure to draw selected last
   for(int i = NUM_WINDOWS-1; i >= 0; i--) {
      if(i != gui_selected_window)
         gui_window_draw(i);
   }
   if(gui_selected_window >= 0)
      gui_window_draw(gui_selected_window);

   // draw toolbar
   gui_drawrect(COLOUR_TOOLBAR, 0, surface.height-TOOLBAR_HEIGHT, surface.width, TOOLBAR_HEIGHT);

   int toolbarPos = 0;
   // padding = 2px
   for(int i = 0; i < NUM_WINDOWS; i++) {
      if(gui_windows[i].minimised) {
         int textWidth = gui_gettextwidth(3);
         int textX = TOOLBAR_ITEM_WIDTH/2 - textWidth/2;
         char text[4] = "   ";
         strcpy_fixed(text, gui_windows[i].title, 3);
         int itemX = TOOLBAR_PADDING+toolbarPos*(TOOLBAR_ITEM_WIDTH+TOOLBAR_PADDING);
         int itemY = surface.height-(TOOLBAR_ITEM_HEIGHT+TOOLBAR_PADDING);
         gui_drawrect(COLOUR_TASKBAR_ENTRY, itemX, itemY, TOOLBAR_ITEM_WIDTH, TOOLBAR_ITEM_HEIGHT);
         gui_writestrat(text, COLOUR_WHITE, itemX+textX, itemY+1);
         gui_windows[i].toolbar_pos = toolbarPos;
         toolbarPos++;
      }
   }
}

void gui_cursor_draw();
void gui_cursor_save_bg();
void gui_redrawall() {
   for(int i = 0; i < NUM_WINDOWS; i++) {
      gui_window_t *window = &gui_windows[i];
      window->needs_redraw = true;
   }
   gui_clear(gui_bg);
   if(desktop_enabled) gui_desktop_draw();
   gui_draw();
   gui_cursor_save_bg();
   if(mouse_enabled) gui_cursor_draw();
}

void mouse_enable();

void gui_interrupt_switchtask(void *regs) {
   // switch to task of selected window to handle interrupt

   int newtask = get_task_from_window(gui_selected_window);

   if(newtask == get_current_task() || newtask == -1) return;

   switch_to_task(newtask, regs);

}

void gui_keypress(char key) {

   if(gui_selected_window >= 0) {
      gui_window_t *selected = &gui_windows[gui_selected_window];
      if(selected->keypress_func != NULL)
         (*(selected->keypress_func))(key, gui_selected_window);
   }
}

void gui_return(void *regs) {
   if(gui_selected_window >= 0) {
      gui_window_t *selected = &gui_windows[gui_selected_window];
      if(selected->return_func != NULL)
         (*(selected->return_func))(regs, gui_selected_window);
   }
}

void gui_backspace() {
   if(gui_selected_window >= 0) {
      gui_window_t *selected = &gui_windows[gui_selected_window];
      if(selected->backspace_func != NULL)
         (*(selected->backspace_func))(gui_selected_window);
   }
}

void gui_uparrow(registers_t *regs) {

   if(gui_selected_window >= 0) {
      gui_window_t *selected = &gui_windows[gui_selected_window];
      if(selected->uparrow_func != NULL) {
         int taskIndex = get_current_task();
         task_state_t *task = &gettasks()[taskIndex];

         if(taskIndex == -1 || !task->enabled || get_current_task_window() != gui_selected_window || selected->uparrow_func == (void *)window_term_uparrow) {
            // launch into function directly as kernel
            //gui_window_writestr("\nCalling function as kernel\n", 0, gui_selected_window);

            (*(selected->uparrow_func))(gui_selected_window);
         } else {
            // run as task
            //gui_window_writestr("\nCalling function as task\n", 0, gui_selected_window);

            task_call_subroutine(regs, (uint32_t)(selected->uparrow_func), NULL, 0);
         }
      }
   }
   
}

void gui_downarrow(registers_t *regs) {

   if(gui_selected_window >= 0) {
      gui_window_t *selected = &gui_windows[gui_selected_window];
      if(selected->downarrow_func != NULL) {
         int taskIndex = get_current_task();
         task_state_t *task = &gettasks()[taskIndex];

         if(taskIndex == -1 || !task->enabled || get_current_task_window() != gui_selected_window || selected->downarrow_func == (void *)window_term_downarrow) {
            // launch into function directly as kernel
            //gui_window_writestr("\nCalling function as kernel\n", 0, gui_selected_window);

            (*(selected->downarrow_func))(gui_selected_window);
         } else {
            // run as task
            //gui_window_writestr("\nCalling function as task\n", 0, gui_selected_window);

            task_call_subroutine(regs, (uint32_t)(selected->downarrow_func), NULL, 0);
         }
      }
   }
   
}

void gui_window_draw(int windowIndex) {
   gui_window_t *window = &gui_windows[windowIndex];
   if(window->closed || window->minimised || window->dragged)
      return;

   uint16_t bg = COLOUR_WHITE;

   if(window->needs_redraw || windowIndex == gui_selected_window) {
      // background
      //gui_drawrect(bg, window->x, window->y, window->width, window->height);

      // titlebar
      gui_drawrect(COLOUR_TITLEBAR, window->x+1, window->y+1, window->width-2, TITLEBAR_HEIGHT);
      // titlebar text, centred
      int titleWidth = gui_gettextwidth(strlen(window->title));
      int titleX = window->x + window->width/2 - titleWidth/2;
      gui_drawrect(COLOUR_WHITE, titleX-4, window->y+1, titleWidth+8, TITLEBAR_HEIGHT);
      gui_writestrat(window->title, 0, titleX, window->y+3);
      // titlebar buttons
      gui_drawcharat('x', 0, window->x+window->width-(FONT_WIDTH+3), window->y+2);
      gui_drawcharat('-', 0, window->x+window->width-(FONT_WIDTH+3)*2, window->y+2);

      if(window->framebuffer != NULL) {
         // draw window content/framebuffer
         for(int y = 0; y < window->height - TITLEBAR_HEIGHT; y++) {
            for(int x = 0; x < window->width; x++) {
               // ignore pixels covered by the currently selected window
               int screenY = (window->y + y + TITLEBAR_HEIGHT);
               int screenX = (window->x + x);
               if(windowIndex != gui_selected_window && gui_selected_window >= 0
               && screenX >= gui_windows[gui_selected_window].x
               && screenX < gui_windows[gui_selected_window].x + gui_windows[gui_selected_window].width
               && screenY >= gui_windows[gui_selected_window].y
               && screenY < gui_windows[gui_selected_window].y + gui_windows[gui_selected_window].height)
                  continue;

               int index = screenY*surface.width + screenX;
               int w_index = y*window->width + x;
               set_framebuffer(index, window->framebuffer[w_index]);
            }
         }
      }
      
      if(windowIndex != gui_selected_window) gui_drawdottedrect(0, window->x, window->y, window->width, window->height);

      window->needs_redraw = false;
   }


   if(windowIndex == gui_selected_window) {

      // current text content/buffer
      gui_drawrect(bg, window->x+1, window->y+window->text_y+TITLEBAR_HEIGHT, window->width-2, FONT_HEIGHT);
      gui_drawcharat('>', 8, window->x + 1, window->y + window->text_y+TITLEBAR_HEIGHT);
      gui_writestrat(window->text_buffer, 0, window->x + 1 + FONT_WIDTH + FONT_PADDING, window->y + window->text_y+TITLEBAR_HEIGHT);
      // prompt
      gui_drawcharat('_', 0, window->x + window->text_x + 1 + FONT_WIDTH + FONT_PADDING, window->y + window->text_y+TITLEBAR_HEIGHT);

      // drop shadow if selected
      gui_drawline(COLOUR_DARK_GREY, window->x+window->width, window->y+2, true, window->height-1);
      gui_drawline(COLOUR_DARK_GREY, window->x+2, window->y+window->height, false, window->width-1);

      gui_drawunfilledrect(gui_rgb16(80,80,80), window->x, window->y, window->width, window->height);
   } else {
   }
}

void gui_desktop_init() {
   // load add window icon
   fat_dir_t *entry = fat_parse_path("/bmp/window.bmp");
   if(entry == NULL) {
      gui_writestr("ICON not found\n", 0);
      return;
   }

   icon_window = fat_read_file(entry->firstClusterNo, entry->fileSize);

   // load background
   entry = fat_parse_path("/bmp/bg16.bmp");
   if(entry == NULL) {
      gui_writestr("BG not found\n", 0);
      return;
   }

   gui_bgimage = fat_read_file(entry->firstClusterNo, entry->fileSize);
   gui_bg = bmp_get_colour(gui_bgimage, 0, 0);

   desktop_enabled = true;
   gui_redrawall();
}

int gui_window_add() {

   int newIndex = -1;

   // find first closed window
   for(int i = 0; i < NUM_WINDOWS; i++) {
      if(gui_windows[i].closed) {
         newIndex = i;
         break;
      }
   }

   // otherwise resize array
   if(newIndex == -1) {
      newIndex = NUM_WINDOWS;
      gui_window_t *windows_new = resize((uint32_t)gui_windows, NUM_WINDOWS*sizeof(gui_window_t), (NUM_WINDOWS+1)*sizeof(gui_window_t));
   
      if(windows_new != NULL) {
         NUM_WINDOWS++;
         gui_windows = windows_new;
      } else {
         gui_window_writestr("Couldn't create window\n", 0, 0);
         return -1;
      }
   }

   if(window_init(&gui_windows[newIndex])) {
      gui_windows[newIndex].title[0] = newIndex+'0';
      gui_selected_window = newIndex;

      return newIndex;
   } else {
      strcpy((char*)gui_windows[newIndex].title, "ERROR");
      gui_windows[newIndex].closed = true;
      gui_window_writestr("Couldn't create window\n", 0, 0);
   }

   return -1;
}

void gui_window_close(void *regs, int windowIndex) {
   gui_window_t *window = &gui_windows[windowIndex];
   if(window->closed) return;

   if(windowIndex == gui_selected_window)
      gui_selected_window = -1;

   end_task(get_task_from_window(windowIndex), regs);

   // re-establish pointer as gui_windows may have been resized in end_task
   window = &gui_windows[windowIndex];
   window->closed = true;

   for(int i = 0; i < CMD_HISTORY_LENGTH; i++) {
      //free((uint32_t)&window->cmd_history[i][0], TEXT_BUFFER_LENGTH); // broken
      window->cmd_history[i] = NULL;
   }
   free((uint32_t)window->framebuffer, window->width*(window->height-TITLEBAR_HEIGHT)*2);
   window->framebuffer = NULL;

   gui_redrawall();
}

void gui_desktop_draw() {
   //gui_writeuint((uint32_t)icon_window, 0);
   int32_t width = bmp_get_width(gui_bgimage);
   int32_t height = bmp_get_height(gui_bgimage);
   int x = (surface.width - width) / 2;
   int y = ((surface.height - TOOLBAR_HEIGHT) - height) / 2;
   bmp_draw(gui_bgimage, (uint16_t *)surface.buffer, surface.width, surface.height, x, y, 0);
   bmp_draw(icon_window, (uint16_t *)surface.buffer, surface.width, surface.height, 10, 10, 1);
}

void gui_desktop_click() {
   if(gui_mouse_x >= 10 && gui_mouse_y >= 10)
      if(gui_mouse_x <= 60 && gui_mouse_y <= 60)
         gui_window_add();
}

void mouse_enable() {
   // https://wiki.osdev.org/Mouse_Input
   // https://wiki.osdev.org/PS/2_Mouse
   // enable ps2 mouse
   
   if(mouse_enabled) return;

   outb(0x64, 0xA8);

   // enable irq12 interrupt by setting status
   outb(0x64, 0x20);
   unsigned char status = (inb(0x60) | 2);
   outb(0x64, 0x60);
   outb(0x60, status);

   // use default mouse settings
   outb(0x64, 0xD4);
   outb(0x60, 0xF6);
   inb(0x60);

   // enable
   outb(0x64, 0xD4);
   outb(0x60, 0xF4);
   inb(0x60);

   gui_cursor_save_bg();
   mouse_enabled = true;
}

void gui_cursor_save_bg() {
   uint16_t *terminal_buffer = (uint16_t*) surface.buffer;
   for(int y = gui_mouse_y; y < gui_mouse_y + FONT_HEIGHT; y++) {
      for(int x = gui_mouse_x; x < gui_mouse_x + FONT_WIDTH; x++) {
         if(x >= 0 && x < (int)surface.width && y >=0 && y < (int)surface.height)
            cursor_buffer[(y-gui_mouse_y)*FONT_WIDTH+(x-gui_mouse_x)] = terminal_buffer[y*(int)surface.width+x];
      }
   }
}

void gui_cursor_restore_bg(int old_x, int old_y) {
   for(int y = old_y; y < old_y + FONT_HEIGHT; y++) {
      for(int x = old_x; x < old_x + FONT_WIDTH; x++) {
         set_framebuffer(y*(int)surface.width+x, cursor_buffer[(y-old_y)*FONT_WIDTH+(x-old_x)]);
      }
   }
}

void gui_cursor_draw() {
   gui_drawcharat(27, 0, gui_mouse_x, gui_mouse_y); // outline
   gui_drawcharat(28, COLOUR_WHITE, gui_mouse_x, gui_mouse_y); // fill

}

void mouse_update(int relX, int relY) {
   int old_x = gui_mouse_x;
   int old_y = gui_mouse_y;

   gui_mouse_x += relX;
   gui_mouse_y -= relY;

   if(gui_mouse_x >= (int)surface.width)
      gui_mouse_x %= surface.width;

   if(gui_mouse_y >= (int)surface.height)
      gui_mouse_y %= surface.height;

   if(gui_mouse_x < 0)
      gui_mouse_x = surface.width + gui_mouse_x;

   if(gui_mouse_y < 0)
      gui_mouse_y = surface.height + gui_mouse_y;

   // just in case
   if(gui_mouse_x < 0 || gui_mouse_x >= (int)surface.width)
      gui_mouse_x = 0;
   if(gui_mouse_y < 0 || gui_mouse_y >= (int)surface.height)
      gui_mouse_y = 0;

   gui_cursor_restore_bg(old_x, old_y); // restore pixels under old cursor location

   gui_cursor_save_bg(); // save pixels at new cursor location

   gui_cursor_draw();
}

bool mouse_clicked_on_window(void *regs, int index) {
   gui_window_t *window = &gui_windows[index];
   // clicked on window's icon in toolbar
   if(window->minimised) {
      if(gui_mouse_x >= TOOLBAR_PADDING+window->toolbar_pos*(TOOLBAR_ITEM_WIDTH+TOOLBAR_PADDING) && gui_mouse_x <= TOOLBAR_PADDING+window->toolbar_pos*(TOOLBAR_ITEM_WIDTH+TOOLBAR_PADDING)+TOOLBAR_ITEM_WIDTH
         && gui_mouse_y >= (int)surface.height-(TOOLBAR_ITEM_HEIGHT+TOOLBAR_PADDING) && gui_mouse_y <= (int)surface.height-TOOLBAR_PADDING) {
            window->minimised = false;
            window->active = true;
            window->needs_redraw = true;
            gui_selected_window = index;
            return true;
      }
   } else if(!window->closed && gui_mouse_x >= window->x && gui_mouse_x <= window->x + window->width
      && gui_mouse_y >= window->y && gui_mouse_y <= window->y + window->height) {

         int relX = gui_mouse_x - window->x;
         int relY = gui_mouse_y - window->y;

         // minimise
         if(relY < TITLEBAR_HEIGHT && relX > window->width - (FONT_WIDTH+3)*2 && relX < window->width - (FONT_WIDTH+3)) {
            window->minimised = true;
            gui_selected_window = -1;
            return false;
         }

         // close
         if(relY < TITLEBAR_HEIGHT && relX > window->width - (FONT_WIDTH+3)) {
            gui_window_close(regs, index);
            return false;
         }


         window->active = true;
         window->needs_redraw = true;
         gui_selected_window = index;
         return true;
   }

   return false;
}

void mouse_leftclick(void *regs, int relX, int relY) {
   // dragging windows
   if(mouse_held) {
      if(gui_selected_window >= 0) {
         gui_window_t *window = &gui_windows[gui_selected_window];
         if(window->active) {
            window->x += relX;
            window->y -= relY;
            if(window->x < 0)
               window->x = 0;
            if(window->x + window->width > (int)surface.width)
               window->x = surface.width - window->width;
            if(window->y < 0)
               window->y = 0;
            if(window->y + window->height > (int)surface.height - TOOLBAR_HEIGHT)
               window->y = surface.height - window->height - TOOLBAR_HEIGHT;

            window->dragged = true;
            window->needs_redraw = true;
            gui_drawdottedrect(COLOUR_WHITE, window->x, window->y, window->width, window->height);
            //gui_draw();
         }
      }
      return;
   }
   
   mouse_held = true;

   int prevSelected = gui_selected_window;
   bool clickedOnWindow = false;

   // check if clicked on current window
   if(mouse_clicked_on_window(regs, gui_selected_window)) {
      // 
      clickedOnWindow = true;
   } else {
      // check other windows

      gui_selected_window = -1;
      for(int i = 0; i < NUM_WINDOWS; i++) {
         if(mouse_clicked_on_window(regs, i)) {
            clickedOnWindow = true;
            break;
         }
      }
   }

   // make all other windows inactive
   for(int i = 0; i < NUM_WINDOWS; i++) {
      if(i != gui_selected_window) {
         gui_window_t *window = &gui_windows[i];
         window->active = false;
         window->needs_redraw = true;
      }
   }

   // only call routine when already the focused window
   if(clickedOnWindow && gui_selected_window == prevSelected) {
      gui_interrupt_switchtask(regs);
      gui_window_t *window = &gui_windows[gui_selected_window];

      if(get_current_task_window() == gui_selected_window && window->click_func != NULL) {
         //gui_window_writestr("\nCalling function as task\n", 0, gui_selected_window);

         uint32_t *args = malloc(sizeof(uint32_t) * 2);
         args[1] = gui_mouse_x - window->x;
         args[0] = gui_mouse_y - (window->y + TITLEBAR_HEIGHT);

         if((gui_mouse_y - (window->y + TITLEBAR_HEIGHT)) >= 0) {
            task_call_subroutine(regs, (uint32_t)(window->click_func), args, 2);
         } else {
            // clicked titlebar, don't call routine
            free((uint32_t)args, sizeof(uint32_t) * 2);
         }
      }

   } else {

      if(desktop_enabled)
         gui_desktop_click();
   
   }

   gui_draw();

}

void mouse_leftrelease() {
   if(mouse_held) {
      if(gui_selected_window >= 0)
         gui_windows[gui_selected_window].dragged = false;

      gui_redrawall();
   }

   mouse_held = false;
}

uint16_t *gui_get_framebuffer() {
   return (uint16_t*)surface.buffer;
}

uint32_t gui_get_framebuffer_size() {
   return surface.width*surface.height*2;
}

gui_window_t *gui_get_windows() {
   return gui_windows;
}

int gui_get_selected_window() {
   return gui_selected_window;
}

void gui_set_selected_window(int windowIndex) {
   gui_selected_window = windowIndex;
}

size_t gui_get_width() {
   return surface.width;
}

size_t gui_get_height() {
   return surface.height;
}

int gui_get_num_windows() {
   return NUM_WINDOWS;
}

uint32_t gui_get_window_framebuffer(int windowIndex) {
   gui_window_t *window = &gui_windows[windowIndex];
   return (uint32_t)window->framebuffer;
}

void gui_showtimer(int number) {
   gui_drawrect(gui_bg, -10, 5, 7, 11);
   gui_writenumat(number, COLOUR_WHITE, -10, 5);
}

int gui_gettextwidth(int textlength) {
   return textlength*(FONT_WIDTH+FONT_PADDING);
}