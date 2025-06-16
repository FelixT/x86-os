// bootloader 1 loader
// located in memory at 0x7e00 size 63.5k
// init text mode terminal, interrupts, ata
// loads kernel to 0x06400000 (KERNEL_START)
// just go ahead and load the kernel to the 'higher half location'
// which simplies enabling paging (can identity map)

#include "ata.h"
#include "terminal.h"
#include "draw.h"
#include "font.h"
#include "surface_t.h"
#include "memory.h"

extern uint8_t videomode;

surface_t surface;

int size = 128000;

extern uint8_t vbe_mode_info_structure;

int gui_font_y = 5;

void cboot_writestr(char *c) {
   if(videomode) {
      // gui
      draw_string(&surface, c, 0xFFFF, 5, gui_font_y);
      gui_font_y += getFont()->height + getFont()->padding_y;
   } else {
      // cli
      terminal_write(c);
   }
}

void cboot_printf(char *format, ...) {
   char *buffer = (char*)malloc(512);
   va_list args;
   va_start(args, format);
   vsprintf(buffer, format, args);
   va_end(args);
   cboot_writestr(buffer);
   free((uint32_t)buffer, 512);
}

void debug_writestr(char *c) {
   cboot_printf("%s", c);
}

void gui_writeuint(uint32_t num, uint16_t colour) {
   (void)colour;
   cboot_printf("%u", num);
}

void gui_writestr(char *c, uint16_t colour) {
   (void)colour;
   cboot_printf("%s", c);
}

void gui_writenum(int num, uint16_t colour) {
   (void)colour;
   cboot_printf("%i", num);
}

void debug_printf(char *format, ...) {
    char *buffer = (char*)malloc(512);
   va_list args;
   va_start(args, format);
   vsprintf(buffer, format, args);
   va_end(args);
   cboot_writestr(buffer);
   free((uint32_t)buffer, 512);
}

void gui_printf(char *format, uint16_t colour, ...) {
   (void)colour;
    char *buffer = (char*)malloc(512);
   va_list args;
   va_start(args, colour);
   vsprintf(buffer, format, args);
   va_end(args);
   cboot_writestr(buffer);
   free((uint32_t)buffer, 512);
}

void cboot() {
   memory_init();

   vbe_mode_info_t *vbe_info = (vbe_mode_info_t*)(&vbe_mode_info_structure);
   // setup text
   if(videomode) {
      // gui, setup surface
      surface.buffer = vbe_info->framebuffer;
      surface.width = vbe_info->width;
      surface.height = vbe_info->height;

      // set black
      for(int i = 0; i < vbe_info->width*vbe_info->height; i++) {
         uint16_t *fb = (uint16_t*)vbe_info->framebuffer;
         fb[i] = 0;
      }

      font_init();
      cboot_printf("Found framebuffer at 0x%h", surface.buffer);
   } else {
      // cli
      terminal_clear();
   }

   cboot_printf("Loading into 0x%h", KERNEL_START);
   ata_identify(true, true);

   uint8_t *bytes = ata_read_exact(true, true, 64000, size);
   int used = 0;
   for(int i = 0; i < KERNEL_HEAP_SIZE/MEM_BLOCK_SIZE; i++) {
      if(memory_get_table()[i].allocated) used++;
   }
   cboot_printf("Loaded at %u\nAllocated %i/%i", bytes, used, KERNEL_HEAP_SIZE/MEM_BLOCK_SIZE);

   cboot_printf("Copying to 0x%h", KERNEL_START);
   memcpy_fast((void*)KERNEL_START, bytes, size);

   cboot_printf("Freeing");
   free((uint32_t)bytes, size);

   cboot_printf("Jumping");
   
   // set videomode in eax, read in cmain
   if(videomode) {
      asm volatile(
         "movl $1, %%ecx\n\t" // videomode = 1
         "movl %0, %%edx\n\t" // surface ptr
         "jmp *%1\n\t"
         :
         : "r" (&surface), "r" ((void*)KERNEL_START)
         : "%ecx", "%edx"
      );
   } else {
      asm volatile(
         "movl $0, %%ecx\n\t" // videomode = 0
         "movl %0, %%edx\n\t" // surface ptr
         "jmp *%1\n\t"
         :
         : "r" (&surface), "r" ((void*)KERNEL_START)
         : "%ecx", "%edx"
      );
   }

}