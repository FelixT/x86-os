// https://wiki.osdev.org/Interrupts_tutorial
// https://wiki.osdev.org/Interrupt_Descriptor_Table

#include "interrupts.h"
#include "window.h"
#include "api.h"
#include "events.h"
#include "ata.h"
#include "windowmgr.h"

extern void* isr_stub_table[];
extern void* irq_stub_table[];
extern void* software_interrupt_routine;

extern int videomode;
extern bool switching; // preemptive multitasking enabled
bool switching_paused = false;

extern void mouse_update(uint32_t relX, uint32_t relY);
extern void mouse_leftclick(registers_t *regs, int relX, int relY);
extern void mouse_leftrelease();

__attribute__((aligned(0x10))) 
static idt_entry_t idt[256];

static idtr_t idtr;

void (*irqs[32])(registers_t *regs);

void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags) {
   idt_entry_t* descriptor = &idt[vector];
 
   descriptor->isr_low = (uint32_t)isr & 0xFFFF;
   descriptor->kernel_cs = 0x08 ; // code selector location in gdt (CODE_SEG)
   descriptor->zero = 0;
   descriptor->attributes = flags;
   descriptor->isr_high = (uint32_t)isr >> 16;
}

void pic_remap() {

   // avoid irq conflicts: the first 32 irqs are reserved by intel for cpu interruptions

   unsigned char a1 = inb(0x21); // save masks
	unsigned char a2 = inb(0xA1);
 
	outb(0x20, 0x11);  // starts the initialization sequence (in cascade mode)
	outb(0xA0, 0x11);

	outb(0x21, 0x20); // set master offset at 32
	outb(0xA1, 0x28); // set slave offset 32+8
	outb(0x21, 0x04); // tell master slaves location at irq2
	outb(0xA1, 0x02); // tell slave its cascade identity
 
	outb(0x21, 0x01);
	outb(0xA1, 0x01);
 
	outb(0x21, a1);   // restore saved masks.
	outb(0xA1, a2);

}

void idt_init() {
   idtr.base = (uintptr_t)&idt[0];
   idtr.limit = (uint16_t)sizeof(idt_entry_t) * 256 - 1;
 
   // note flag 0x8E = 32bit interrupt gate, 0x8F = 32bit trap gate
   // https://wiki.osdev.org/IDT#Gate_Types

   // exceptions
   for(uint8_t vector = 0; vector < 32; vector++)
      idt_set_descriptor(vector, isr_stub_table[vector], 0x8F);

   for(uint8_t vector = 32; vector < 49; vector++)
      idt_set_descriptor(vector, irq_stub_table[vector-32], 0x8E);

   for(uint8_t irq = 0; irq < 32; irq++)
      irqs[irq] = NULL;

   idt_set_descriptor(48, irq_stub_table[48-32], 0xEE); // software interrupt 0x30, can be called from ring 3
 
   __asm__ volatile("lidt %0" : : "m"(idtr)); // load idt

   pic_remap();

   __asm__ volatile("sti"); // set the interrupt flag (enable interrupts)

}

char scan_to_char(int scan_code) {
   // https://www.millisecond.com/support/docs/current/html/language/scancodes.htm

   char upperFirst[13] = "1234567890-=";
   char upperSecond[13] = "QWERTYUIOP[]";
   char upperThird[13] = "ASDFGHJKL;'#";
   char upperFourth[12] = "\\ZXCVBNM,./";

   if(scan_code >= 2 && scan_code <= 13)
      return upperFirst[scan_code-2];

   if(scan_code >= 16 && scan_code <= 27)
      return upperSecond[scan_code-16];

   if(scan_code >= 30 && scan_code <= 41)
      return upperThird[scan_code-30];

   if(scan_code >= 43 && scan_code <= 53)
      return upperFourth[scan_code-43];

   if(scan_code == 57)
      return ' ';

   return '\0';
}

extern void bmp_draw(uint8_t *bmp, uint16_t* framebuffer, int screenWidth, int screenHeight, int x, int y, bool whiteIsTransparent);

void register_irq(int index, void (*handler)(registers_t *regs)) {
   irqs[index] = handler;
}

void software_handler(registers_t *regs) {

   if(regs->eax == 1)
      api_write_string(regs);

   if(regs->eax == 2)
      api_write_number(regs);

   if(regs->eax == 3)
      api_yield(regs);

   if(regs->eax == 4)
      api_print_program_stack(regs);

   if(regs->eax == 5)
      api_print_stack(regs);

   if(regs->eax == 6)
      api_write_uint(regs);

   if(regs->eax == 7)
      api_return_framebuffer(regs);

   if(regs->eax == 8)
      api_write_newline(regs);

   if(regs->eax == 9)
      api_redraw_window(regs);

   if(regs->eax == 10)
      api_end_task(regs);

   if(regs->eax == 11)
      api_override_uparrow(regs);

   if(regs->eax == 12)
      api_end_subroutine(regs);

   if(regs->eax == 13)
      api_override_mouseclick(regs);

   if(regs->eax == 14)
      api_return_window_width(regs);

   if(regs->eax == 15)
      api_return_window_height(regs);

   if(regs->eax == 16)
      api_malloc(regs);

   if(regs->eax == 17)
      api_fat_get_bpb(regs);

   if(regs->eax == 18)
      api_fat_get_bpb(regs);

   if(regs->eax == 19)
      api_fat_parse_path(regs);

   if(regs->eax == 20)
      api_fat_read_file(regs);

   if(regs->eax == 21)
      api_draw_bmp(regs);

   if(regs->eax == 22)
      api_write_string_at(regs);

   if(regs->eax == 23)
      api_clear_window(regs);

   if(regs->eax == 24)
      api_get_get_dir_size(regs);

   if(regs->eax == 25)
      api_read_dir(regs);

   if(regs->eax == 26)
      api_get_get_dir_size(regs);
   
   if(regs->eax == 27) 
      api_override_downarrow(regs);

   if(regs->eax == 28)
      api_write_number_at(regs);

   if(regs->eax == 29)
      api_override_draw(regs);

   if(regs->eax == 30)
      api_queue_event(regs);
}  

void keyboard_handler(registers_t *regs) {
   unsigned char scan_code = inb(0x60);

   if(videomode == 1) gui_interrupt_switchtask(regs);

   if(videomode == 0) {

      if(scan_code == 28)
         terminal_return();
      else if(scan_code == 14)
         terminal_backspace();
      else
         terminal_keypress(scan_to_char(scan_code));

   } else {
      gui_keypress(regs, scan_code);
   }

}

int mouse_cycle = 0;
uint8_t mouse_data[3];

void mouse_handler(registers_t *regs) {
   // mouse!
   mouse_data[mouse_cycle] = inb(0x60);
   
   mouse_cycle++;

   if(mouse_cycle == 3) {
      int8_t xm = mouse_data[2];
      int8_t ym = mouse_data[0];

      // handle case of negative relative values
      //int relX = xm - ((mouse_data[1] << 4) & 0x100);
      //int relY = ym - ((mouse_data[1] << 3) & 0x100);

      mouse_update(xm, ym);

      if(mouse_data[1] & 0x1) {
         mouse_leftclick(regs, xm, ym);
      } else {
         mouse_leftrelease();
      }

      mouse_cycle = 0;
   }
}

int timer_i = 0;

void timer_handler(registers_t *regs) {

   if(videomode == 0) {
      terminal_writenumat(timer_i%10, 79);
   } else {
      gui_showtimer(timer_i%10);

      if(timer_i%20 == 0)
         gui_draw();

      if(timer_i%40 == 0) {
         if(switching_paused) {
            if(switching) window_writestr(" SP", 0, 0);
         } else {
            switch_task(regs);
         }
      }
   }

   events_check(regs);

   timer_i++;
   timer_i%=10000000;

}

void exception_handler(int int_no, registers_t *regs) {

   if(int_no < 32) {

      // https://wiki.osdev.org/Exceptions
      if(videomode == 1) {

         if(int_no == 14) {
            // page error
            uint32_t addr;
	         asm volatile("mov %%cr2, %0" : "=r" (addr));
            window_writestr("Page fault at ", gui_rgb16(255, 100, 100), 0);
            debug_writehex(addr);
            if(page_getphysical(addr) != (uint32_t)-1) {
               window_writestr(" <", gui_rgb16(255, 100, 100), 0);
               debug_writehex(page_getphysical(addr));
               window_writestr(">", gui_rgb16(255, 100, 100), 0);
            }
            window_writestr(" with eip ", gui_rgb16(255, 100, 100), 0);
            debug_writehex(regs->eip);
            if(page_getphysical(regs->eip) != (uint32_t)-1) {
               window_writestr(" <", gui_rgb16(255, 100, 100), 0);
               debug_writehex(page_getphysical(regs->eip));
               window_writestr(">", gui_rgb16(255, 100, 100), 0);
            }
            window_writestr("\n", 0, 0);

            //while(true);
         }

         gui_drawrect(gui_rgb16(255, 0, 0), 60, 0, 8*2, 11);
         gui_writenumat(int_no, gui_rgb16(255, 200, 200), 60, 0);

         gui_drawrect(gui_rgb16(255, 0, 0), 200, 0, 8*8, 11);
         gui_writenumat(regs->eip, gui_rgb16(255, 200, 200), 200, 0);
      
         window_writestr("Task ", gui_rgb16(255, 100, 100), 0);
         window_writenum(get_current_task(), 0, 0);
         window_writestr(" ended due to exception ", gui_rgb16(255, 100, 100), 0);
         window_writenum(int_no, 0, 0);
         window_writestr("\n", 0, 0);

         end_task(get_current_task(), regs);
      }
         
   } else {
      // IRQ numbers: https://www.computerhope.com/jargon/i/irq.htm

      int irq_no = int_no - 32;

      switch(irq_no) {
         case 0:
            timer_handler(regs);
            break;
         case 1:
            keyboard_handler(regs);
            break;
         case 12:
            mouse_handler(regs);
            break;
         case 14:
            ata_interrupt(); // acknowledge, that's it
            break;
         case 16:
            // 0x30: software interrupt
            // uses eax, ebx, ecx as potential arguments
            software_handler(regs);
            break;
         default:
            // other interrupt no, print it
            if(videomode == 0) {
               terminal_writeat("  ", 0);
               terminal_writenumat(int_no, 0);
            } else {
               gui_drawrect(COLOUR_CYAN, 0, 0, 7*2, 7);
               gui_writenumat(int_no, 0, 0, 0);
            }
      }

   }

   // send end of command code 0x20 to pic
   if(int_no >= 8)
      outb(0xA0, 0x20); // slave command

   outb(0x20, 0x20); // master command

}

void err_exception_handler(int int_no, registers_t *regs) {

   switching_paused = true;

   window_writestr("Exception ", gui_rgb16(255, 100, 100), 0);
   window_writeuint(int_no, 0, 0);
   window_writestr(" with err code ", gui_rgb16(255, 100, 100), 0);
   window_writeuint(regs->err_code, 0, 0);
   window_writestr(" with eip ", gui_rgb16(255, 100, 100), 0);
   debug_writehex(regs->eip);  
   window_writestr("\n", gui_rgb16(255, 100, 100), 0);

   exception_handler(int_no, regs);

   switching_paused = false;

}