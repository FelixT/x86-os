// https://wiki.osdev.org/Interrupts_tutorial
// https://wiki.osdev.org/Interrupt_Descriptor_Table

#include "interrupts.h"

__attribute__((aligned(0x10))) 
static idt_entry_t idt[256];

static idtr_t idtr;

// https://wiki.osdev.org/Inline_Assembly/Examples
static inline void outb(uint16_t port, uint8_t val) {
   asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
   uint8_t ret;
   asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
   return ret;
}

void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags) {
   idt_entry_t* descriptor = &idt[vector];
 
   descriptor->isr_low = (uint32_t)isr & 0xFFFF;
   descriptor->kernel_cs = 0x08 ; // code selector location in gdt (CODE_SEG)
   descriptor->zero = 0;
   descriptor->attributes = flags;
   descriptor->isr_high = (uint32_t)isr >> 16;
}

extern void* isr_stub_table[];
extern void* irq_stub_table[];
extern void* software_interrupt_routine;

extern int videomode;
extern bool switching; // preemptive multitasking enabled

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
   for (uint8_t vector = 0; vector < 32; vector++) {
      idt_set_descriptor(vector, isr_stub_table[vector], 0x8F);
   }

   for (uint8_t vector = 32; vector < 49; vector++) {
      idt_set_descriptor(vector, irq_stub_table[vector-32], 0x8E);
   }

   idt_set_descriptor(48, irq_stub_table[48-32], 0xEE); // software interrupt 0x30, can be called from ring 3
 
   __asm__ volatile("lidt %0" : : "m"(idtr)); // load idt

   // mask all interrupts except keyboard
   // outb(0x21,0xfd); // master data
   // outb(0xa1,0xff); // slave data

   pic_remap();

   __asm__ volatile("sti"); // set the interrupt flag (enable interrupts)

}

void tss_init() {
   // setup tss entry in gdt
   extern gdt_entry_t gdt_tss;
   extern uint32_t tss_start;
   extern uint32_t tss_end;
   //extern tss_t tss_start;

   uint32_t base = (uint32_t)&tss_start;
   uint32_t limit = (uint32_t)&tss_end;
   
   //uint32_t tss_size = &tss_end - &tss_start;

   uint8_t gran = 0x00;

   gdt_tss.base_low = (base & 0xFFFF);
	gdt_tss.base_middle = (base >> 16) & 0xFF;
	gdt_tss.base_high = (base >> 24) & 0xFF;

   gdt_tss.limit_low = (limit & 0xFFFF);
	gdt_tss.granularity = ((limit >> 16) & 0x0F);
   gdt_tss.granularity |= (gran & 0xF0);

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

void software_handler(registers_t *regs) {
   if(regs->eax == 1) {
      // WRITE STRING...
      // ebx contains string address
      //gui_writestr((char*)regs->ebx, 0);
      gui_window_writestr((char*)regs->ebx, 0, get_current_task_window());
   }

   if(regs->eax == 2) {
      // WRITE NUMBER...
      // ebx contains int
      gui_writenum(regs->ebx, 0);
      gui_writenumat(regs->ebx, 14, 60, 20);
   }

   if(regs->eax == 3) {
      // yield

      switch_task(regs);
   }

   if(regs->eax == 4) {
      // show program stack contents
      for(int i = 0; i < 64; i++) {
         gui_writenum(((int*)regs->esp)[i], 0);
         gui_writestr(" ", 0);
      }
   }

   if(regs->eax == 5) {
      // show current stack contents
      int esp;
      asm("movl %%esp, %0" : "=r"(esp));

      for(int i = 0; i < 64; i++) {
         gui_writenum(((int*)esp)[i], 0);
         gui_writestr(" ", 0);
      }
   }

   if(regs->eax == 6) {
      // print uint ebx to current window
      gui_window_writeuint(regs->ebx, 0, get_current_task_window());
      //gui_window_draw(regs->ecx);
   }

   if(regs->eax == 7) {
      // returns framebuffer address in ebx
      regs->ecx = (uint32_t)gui_get_window_framebuffer(get_current_task_window());
      gui_window_writeuint(regs->ecx, 0, 0);

      //for(int i = 0; i < 10000; i++)
      //   ((uint16_t*) regs->ecx)[i] = 0;
      gui_window_drawchar('\n', 0, 0);
   }

   if(regs->eax == 8) {
      // draw newline char
      gui_window_drawchar('\n', 0, get_current_task_window());
   }

   if(regs->eax == 9) {
      // draw
      gui_get_windows()[get_current_task_window()].needs_redraw = true;
      gui_window_draw(get_current_task_window());
   }

   if(regs->eax == 10) {
      // end task/return
      gui_window_writestr("Task ended with status ", 0, get_current_task_window());
      gui_window_writenum(regs->ebx, 0, get_current_task_window());
      gui_window_drawchar('\n', 0, get_current_task_window());

      end_current_task(regs);
   }
}

void keyboard_handler(registers_t *regs) {
   unsigned char scan_code = inb(0x60);

   if(scan_code == 28)  { // return

      if(videomode == 0)
         terminal_return();
      else
         gui_return(regs);

   } else if(scan_code == 14) { // backspace
      
      if(videomode == 0)
         terminal_backspace();
      else
         gui_backspace();

   } else if(scan_code == 72) { // up arrow
      if(videomode == 1)
         gui_uparrow();
   } else if(scan_code == 80) { // up arrow
      if(videomode == 1)
         gui_downarrow();
   } else {
      // other key pressed

      char c = scan_to_char(scan_code);

      if(videomode == 1)
         gui_keypress(c);
      else
         terminal_keypress(c);
   }
}

int mouse_cycle = 0;
uint8_t mouse_data[3];

void mouse_handler() {
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
         mouse_leftclick(xm, ym);
      } else {
         mouse_leftrelease();
      }

      mouse_cycle = 0;
   }
}

int timer_i = 0;

void timer_handler(registers_t *regs) {
   if(videomode == 0) {
      terminal_writenumat(timer_i, 79);
   } else {
      gui_drawrect(COLOUR_CYAN, -10, 5, 7, 11);
      gui_writenumat(timer_i, COLOUR_WHITE, -10, 5);

      if(timer_i == 0)
         gui_draw();

      if(switching)
         switch_task(regs);
   }

   timer_i++;
   timer_i%=10;

}

void exception_handler(int int_no, registers_t *regs) {

   if(int_no < 32) {

      // https://wiki.osdev.org/Exceptions
      if(videomode == 1) {
         gui_drawrect(gui_rgb16(255, 0, 0), 60, 0, 8*2, 11);
         gui_writenumat(int_no, gui_rgb16(255, 200, 200), 60, 0);

         gui_drawrect(gui_rgb16(255, 0, 0), 200, 0, 8*6, 11);
         gui_writenumat(regs->eip, gui_rgb16(255, 200, 200), 200, 0);
      }
         
   } else {
      // IRQ numbers: https://www.computerhope.com/jargon/i/irq.htm

      int irq_no = int_no - 32;

      if(irq_no == 0) {
         // system timer
         
         timer_handler(regs);         
      } else if(irq_no == 1) {
         // keyboard
         keyboard_handler(regs);
      } else if(irq_no == 12) {

         mouse_handler();

      } else if(irq_no == 16) {

         // 0x30: software interrupt
         // uses eax, ebx, ecx as potential arguments

         software_handler(regs);
      } else {

         // other interrupt no
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
   gui_window_writenum(regs->err_code, 0, 0);
   gui_window_writestr(" ", 0, 0);

   exception_handler(int_no, regs);
}