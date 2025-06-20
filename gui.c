#include "gui.h"
#include "window.h"
#include "windowmgr.h"
#include "font.h"
#include "events.h"
#include "lib/string.h"

extern int videomode;

int gui_mouse_x = 0;
int gui_mouse_y = 0;

uint16_t gui_bg;

bool mouse_enabled = false;
bool mouse_held = false;
bool mouse_heldright = false;
bool cursor_resize = false;
bool gui_cursor_shown = false;

surface_t surface;

uint16_t cursor_buffer[MAX_FONT_WIDTH*MAX_FONT_HEIGHT]; // store whats behind cursor so it can be restored
int cursor_oldx = 0;
int cursor_oldy = 0;

uint16_t *draw_buffer;

static inline void set_framebuffer(int index, uint16_t colour) {
   if(index < 0 || index >= (int)surface.width*(int)surface.height) {
      //window_writestr("Attempted to write outside framebuffer bounds\n", 0, 0);
   } else {
      ((uint16_t*)surface.buffer)[index] = colour;
   }
}

inline uint16_t gui_rgb16(uint8_t r, uint8_t g, uint8_t b) {
   // 5r 6g 5b
   return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

void gui_drawrect(uint16_t colour, int x, int y, int width, int height) {
   draw_rect(&surface, colour, x, y, width, height);
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

void gui_printf(char *format, uint16_t colour, ...) {
   va_list args;
   va_start(args, colour);
   char buffer[256];
   vsnprintf(buffer, 256, format, args);
   gui_writestr(buffer, colour);
   va_end(args);
}

void gui_writestrat(char *c, uint16_t colour, int x, int y) {
   int i = 0;
   while(c[i] != '\0') {
      gui_drawcharat(c[i++], colour, x, y);
      x+=getFont()->width+getFont()->padding;
   }
}

void gui_writenum(int num, uint16_t colour) {
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

extern int *font_letter;

void gui_init_meat(registers_t *regs, void *msg) {
   // needs to be run from interrupt routine with interrupts disabled
   (void)msg;
   gui_writestr("Enabling ATA HD\n", COLOUR_ORANGE);
   ata_identify(true, true);
   gui_writestr("\nEnabling FAT\n", COLOUR_ORANGE);
   fat_setup();
   gui_printf("\nFramebuffer at 0x%h", 0, surface.buffer);
   gui_writestr("\nEnabling paging\n", COLOUR_ORANGE);
   page_init();
   gui_writestr("\nEnabling desktop\n", COLOUR_ORANGE);
   desktop_init();
   gui_writestr("\nEnabling tasks\n", COLOUR_ORANGE);
   tasks_init(regs);
   gui_writestr("\nSet up timer\n", COLOUR_ORANGE);
   timer_set_hz(30);

   getSelectedWindow()->minimised = true;
   getSelectedWindow()->active = false;
   setSelectedWindowIndex(-1);
   gui_redrawall();
}

void gui_init(void) {
   videomode = 1;

   //gui_bg = COLOUR_CYAN;
   gui_bg = COLOUR_BLACK;

   extern uintptr_t surface_boot;
   memcpy(&surface, (void*)surface_boot, sizeof(surface_t));
   //surface.buffer-=128; // ??????? horrible alignment issue

   draw_buffer = (uint16_t*)malloc(sizeof(uint16_t) * surface.width * surface.height);
   font_letter = (int*)malloc(1);

   // reserve framebuffer memory so malloc can't assign it
   memory_reserve(surface.buffer, (int)surface.width*(int)surface.height);
   
   //gui_clear(gui_bg);
   font_init();
   windowmgr_init();
   mouse_enable();

   events_add(1, &gui_init_meat, NULL, -1);
}

void gui_redrawall() {
   // draw to buffer
   uint32_t framebuffer = surface.buffer;
   surface.buffer = (uint32_t)draw_buffer;

   gui_clear(gui_bg);
   desktop_draw();
   windowmgr_redrawall();
   toolbar_draw();

   // copy to display
   surface.buffer = framebuffer;
   memcpy_fast((void*)surface.buffer, (void*)draw_buffer, sizeof(uint16_t) * surface.width * surface.height);

   gui_cursor_shown = false;
   gui_cursor_save_bg();
   if(mouse_enabled) gui_cursor_draw();
}

extern bool switching;
bool gui_interrupt_switchtask(void *regs) {
   // switch to task of selected window to handle interrupt
   if(!switching) return false; // in case we end up here before tasks are enabled

   int newtask = get_task_from_window(getSelectedWindowIndex());
   if(newtask == -1) return false;

   if(newtask == get_current_task()) return true;

   if(!switch_to_task(newtask, regs)) {
      debug_printf("Task %u for window %u is stopped\n", newtask, getSelectedWindowIndex());
      return false;
   }

   return true;
}

void gui_keypress(void *regs, char scan_code) {
   windowmgr_keypress(regs, scan_code);
}

void gui_draw_window(int windowIndex) {
   window_draw(getWindow(windowIndex));
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

   //gui_cursor_save_bg();
   mouse_enabled = true;
}

void gui_cursor_save_bg() {
   if(gui_cursor_shown) return;
   uint16_t *terminal_buffer = (uint16_t*) surface.buffer;
   for(int y = gui_mouse_y; y < gui_mouse_y + getFont()->height; y++) {
      for(int x = gui_mouse_x; x < gui_mouse_x + getFont()->width; x++) {
         if(x >= 0 && x < (int)surface.width && y >=0 && y < (int)surface.height)
            cursor_buffer[(y-gui_mouse_y)*(getFont()->width)+(x-gui_mouse_x)] = terminal_buffer[y*(int)surface.width+x];
      }
   }

   cursor_oldx = gui_mouse_x;
   cursor_oldy = gui_mouse_y;
}

void gui_cursor_restore_bg() {
   for(int y = cursor_oldy; y < cursor_oldy + getFont()->height; y++) {
      for(int x = cursor_oldx; x < cursor_oldx + getFont()->width; x++) {
         set_framebuffer(y*(int)surface.width+x, cursor_buffer[(y-cursor_oldy)*(getFont()->width)+(x-cursor_oldx)]);
      }
   }
   gui_cursor_shown = false;
}

void gui_draw(void) {
   gui_cursor_restore_bg(); // Always restore old cursor first
   windowmgr_draw();

   gui_cursor_save_bg();    // Save new background before drawing cursor
   if(mouse_enabled) gui_cursor_draw();
}

void gui_cursor_draw() {
   if(gui_cursor_shown) return;
   char outline = 27;
   char fill = 28;
   if(cursor_resize) {
      outline = 29;
      fill = 30;
   }

   gui_drawcharat(outline, 0, gui_mouse_x, gui_mouse_y);
   gui_drawcharat(fill, COLOUR_WHITE, gui_mouse_x, gui_mouse_y);
   gui_cursor_shown = true;
}

void mouse_update(int relX, int relY) {
   if(relX == 0 && relY == 0) return;

   gui_mouse_x += relX;
   gui_mouse_y -= relY;
   gui_mouse_x = (gui_mouse_x + surface.width) % surface.width;
   gui_mouse_y = (gui_mouse_y + surface.height) % surface.height;

   gui_cursor_restore_bg(); // restore pixels under old cursor location
   windowmgr_mousemove(gui_mouse_x, gui_mouse_y);

   gui_cursor_save_bg();    // save new background at new position
   if(mouse_enabled) gui_cursor_draw();
}

void mouse_leftclick(void *regs, int relX, int relY) {
   // dragging windows
   if(mouse_held) {
      windowmgr_dragged(regs, relX, relY);
   } else {
      mouse_held = true;

      if(!windowmgr_click(regs, gui_mouse_x, gui_mouse_y))
         desktop_click(regs, gui_mouse_x, gui_mouse_y);

      //gui_draw();
   }

}

void mouse_release(registers_t *regs) {
   bool redraw = false;

   if(mouse_held) {
      gui_window_t *window = getSelectedWindow();
      if(window) {
         if(window->dragged) {
            redraw = true;
            window->dragged = false;
         }
         if(window->resized) {
            redraw = true;
            window_resize(regs, window, window->width, window->height);
            window->resized = false;
         } else {
            window_mouserelease(regs, window);
         }
      }
   }
   //if(mouse_heldright)
   //   redraw = true;

   if(redraw) gui_redrawall();

   mouse_held = false;
   mouse_heldright = false;
}

void mouse_rightclick(void *regs) {
   if(mouse_heldright) {

   } else {
      windowmgr_rightclick(regs, gui_mouse_x, gui_mouse_y);
      mouse_heldright = true;
   }
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
   return textlength*(getFont()->width+getFont()->padding) - getFont()->padding;
}