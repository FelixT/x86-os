#include "gui.h"
#include "window.h"
#include "windowmgr.h"

extern int videomode;

int gui_mouse_x = 0;
int gui_mouse_y = 0;

uint16_t gui_bg;

bool mouse_enabled = false;
bool mouse_held = false;

surface_t surface;

uint16_t cursor_buffer[FONT_WIDTH*FONT_HEIGHT]; // store whats behind cursor so it can be restored

bool desktop_enabled = false;
uint8_t *icon_window;
uint8_t *gui_bgimage;

static inline void set_framebuffer(int index, uint16_t colour) {
   if(index < 0 || index >= (int)surface.width*(int)surface.height) {
      //window_writestr("Attempted to write outside framebuffer bounds\n", 0, 0);
   } else {
      ((uint16_t*)surface.buffer)[index] = colour;
   }
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

void gui_redrawall();
void gui_drawchar(char c, uint16_t colour) {

   if(getSelectedWindowIndex() < 0)
      return;

   window_drawchar(c, colour, getSelectedWindowIndex());

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
   windowmgr_init();
}

void gui_desktop_draw();
void gui_draw_window(int windowIndex);
void gui_draw(void) {
   //gui_clear(3);   
   // make sure to draw selected last
   for(int i = getWindowCount()-1; i >= 0; i--) {
      if(i != getSelectedWindowIndex())
         gui_draw_window(i);
   }
   if(getSelectedWindowIndex() >= 0)
      gui_draw_window(getSelectedWindowIndex());

   // draw taskbar/toolbar
   toolbar_draw();
}

void gui_cursor_draw();
void gui_cursor_save_bg();
void gui_redrawall() {
   for(int i = 0; i < getWindowCount(); i++) {
      gui_window_t *window = getWindow(i);
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

   int newtask = get_task_from_window(getSelectedWindowIndex());

   if(newtask == get_current_task() || newtask == -1) return;

   switch_to_task(newtask, regs);

}

void gui_keypress(void *regs, char scan_code) {
   windowmgr_keypress(regs, scan_code);
}

void gui_draw_window(int windowIndex) {
   window_draw(windowIndex);
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

void gui_window_close(void *regs, int windowIndex) {
   gui_window_t *window = getWindow(windowIndex);
   if(window->closed) return;

   if(windowIndex == getSelectedWindowIndex())
      setSelectedWindowIndex(-1);

   end_task(get_task_from_window(windowIndex), regs);

   // re-establish pointer as gui_windows may have been resized in end_task
   window = getWindow(windowIndex);
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
   gui_window_t *window = getWindow(index);
   // clicked on window's icon in toolbar
   if(gui_mouse_x >= TOOLBAR_PADDING+window->toolbar_pos*(TOOLBAR_ITEM_WIDTH+TOOLBAR_PADDING) && gui_mouse_x <= TOOLBAR_PADDING+window->toolbar_pos*(TOOLBAR_ITEM_WIDTH+TOOLBAR_PADDING)+TOOLBAR_ITEM_WIDTH
      && gui_mouse_y >= (int)surface.height-(TOOLBAR_ITEM_HEIGHT+TOOLBAR_PADDING) && gui_mouse_y <= (int)surface.height-TOOLBAR_PADDING) {
         window->minimised = false;
         window->active = true;
         window->needs_redraw = true;
         setSelectedWindowIndex(index);
         return true;
   } else if(!window->closed && gui_mouse_x >= window->x && gui_mouse_x <= window->x + window->width
      && gui_mouse_y >= window->y && gui_mouse_y <= window->y + window->height) {

         int relX = gui_mouse_x - window->x;
         int relY = gui_mouse_y - window->y;

         // minimise
         if(relY < TITLEBAR_HEIGHT && relX > window->width - (FONT_WIDTH+3)*2 && relX < window->width - (FONT_WIDTH+3)) {
            window->minimised = true;
            setSelectedWindowIndex(-1);
            return false;
         }

         // close
         if(relY < TITLEBAR_HEIGHT && relX > window->width - (FONT_WIDTH+3)) {
            gui_window_close(regs, index);
            return false;
         }


         window->active = true;
         window->needs_redraw = true;
         setSelectedWindowIndex(index);
         return true;
   }

   return false;
}

void mouse_leftclick(void *regs, int relX, int relY) {
   // dragging windows
   if(mouse_held) {
      if(getSelectedWindowIndex() >= 0) {
         gui_window_t *window = getSelectedWindow();
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

   int prevSelected = getSelectedWindowIndex();
   bool clickedOnWindow = false;

   // check if clicked on current window
   if(mouse_clicked_on_window(regs, getSelectedWindowIndex())) {
      // 
      clickedOnWindow = true;
   } else {
      // check other windows

      setSelectedWindowIndex(-1);
      for(int i = 0; i < getWindowCount(); i++) {
         if(mouse_clicked_on_window(regs, i)) {
            clickedOnWindow = true;
            break;
         }
      }
   }

   // make all other windows inactive
   for(int i = 0; i < getWindowCount(); i++) {
      if(i != getSelectedWindowIndex()) {
         gui_window_t *window = getWindow(i);
         window->active = false;
         window->needs_redraw = true;
      }
   }

   // only call routine when already the focused window
   if(clickedOnWindow && getSelectedWindowIndex() == prevSelected) {
      gui_interrupt_switchtask(regs);
      gui_window_t *window = getSelectedWindow();

      if(get_current_task_window() == getSelectedWindowIndex() && window->click_func != NULL) {
         //window_writestr("\nCalling function as task\n", 0, gui_selected_window);

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
      if(getSelectedWindowIndex() >= 0)
         getSelectedWindow()->dragged = false;

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
   return (void*)getWindow(0);
}

size_t gui_get_width() {
   return surface.width;
}

size_t gui_get_height() {
   return surface.height;
}

surface_t *gui_get_surface() {
   return &surface;
}

uint32_t gui_get_window_framebuffer(int windowIndex) {
   gui_window_t *window = getWindow(windowIndex);
   return (uint32_t)window->framebuffer;
}

void gui_showtimer(int number) {
   gui_drawrect(gui_bg, -10, 5, 7, 11);
   gui_writenumat(number, COLOUR_WHITE, -10, 5);
}

int gui_gettextwidth(int textlength) {
   return textlength*(FONT_WIDTH+FONT_PADDING);
}