// https://wiki.osdev.org/Interrupts_tutorial
// https://wiki.osdev.org/Interrupt_Descriptor_Table

#include "interrupts.h"

extern void* isr_stub_table[];
extern void* irq_stub_table[];
extern void* software_interrupt_routine;

extern int videomode;
extern bool switching; // preemptive multitasking enabled

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

   if(regs->eax == 1) {
      // WRITE STRING...
      // IN: ebx = string address
      if(gettasks()[get_current_task()].vmem_start == 0) // not elf
         gui_window_writestr((char*)(gettasks()[get_current_task()].prog_entry+regs->ebx), 0, get_current_task_window());
      else // elf
         gui_window_writestr((char*)regs->ebx, 0, get_current_task_window());
   }

   if(regs->eax == 2) {
      // WRITE NUMBER...
      // IN: ebx = int

      gui_window_writenum(regs->ebx, 0, get_current_task_window());
      //gui_writenum(regs->ebx, 0);
   }

   if(regs->eax == 3) {
      // yield

      switch_task(regs);
   }

   if(regs->eax == 4) {
      // show program stack contents

      for(int i = 0; i < 64; i++) {
         gui_writenum(((int*)regs->esp)[i], get_current_task_window());
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
   }

   if(regs->eax == 7) {
      // returns framebuffer address in ebx
      regs->ebx = (uint32_t)gui_get_window_framebuffer(get_current_task_window());
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

      end_task(get_current_task(), regs);
   }

   if(regs->eax == 11) {
      // override uparrow window function

      uint32_t addr = regs->ebx;

      gui_window_writestr("Overriding uparrow function\n", 0, get_current_task_window());

      gui_get_windows()[get_current_task_window()].uparrow_func = (void *)(addr);
   }

   if(regs->eax == 12) {
      // end subroutine i.e. an uparrow function call

      task_subroutine_end(regs) ;
   }

   if(regs->eax == 13) {
      // override mouse click function

      uint32_t addr = regs->ebx;

      gui_window_writestr("Overriding click function\n", 0, get_current_task_window());

      gui_get_windows()[get_current_task_window()].click_func = (void *)(addr);
   }

   if(regs->eax == 14) {
      // get window width
      regs->ebx = gui_get_windows()[get_current_task_window()].width;
   }

   if(regs->eax == 15) {
      // get window (framebuffer) height
      regs->ebx = gui_get_windows()[get_current_task_window()].height - TITLEBAR_HEIGHT;
   }

   if(regs->eax == 16) {
      // malloc
      // OUT: ebx = addr
      uint32_t *mem = malloc(1); // 4K
      regs->ebx = (uint32_t)mem;

      // TODO: use special usermode malloc rather than the kernel malloc
   }

   if(regs->eax == 17) {
      // fat get bpb

      fat_bpb_t *bpb = malloc(sizeof(fat_bpb_t));
      *bpb = fat_get_bpb(); // refresh fat tables

      regs->ebx = (uint32_t)bpb;

   }

   if(regs->eax == 18) {
      // fat get root

      fat_dir_t *items = malloc(32 * fat_get_bpb().noRootEntries);
      fat_read_root(items);

      regs->ebx = (uint32_t)items;

   }

   if(regs->eax == 19) {
      // fat parse path
      // IN: ebx = addr of char* path
      // OUT: ebx = addr of fat_dir_t entry for path or 0 if doesn't exist

      fat_dir_t *entry = fat_parse_path((char*)regs->ebx);
      regs->ebx = (uint32_t)entry;

   }

   if(regs->eax == 20) {
      // fat read file
      // IN: ebx = first cluster no
      // IN: ecx = file size
      // OUT: ebx = addr of file content buffer

      uint8_t *content = fat_read_file(regs->ebx, regs->ecx);

      regs->ebx = (uint32_t)content;

   }

   if(regs->eax == 21) {
      // draw bmp
      // IN: ebx = bmp address
      // IN: ecx = x
      // IN: edx = y
      gui_window_t *window = &gui_get_windows()[get_current_task_window()];

      bmp_draw((uint8_t*)regs->ebx, window->framebuffer, window->width, window->height - TITLEBAR_HEIGHT, regs->ecx, regs->edx, false);

   }

   if(regs->eax == 22) {
      // write str at
      // IN: ebx = string address
      // IN: ecx = x
      // IN: edx = y
      gui_window_writestrat((char*)regs->ebx, 0, regs->ecx, regs->edx, get_current_task_window());

   }

   if(regs->eax == 23) {
      // clear window
      // IN: ebx = colour
      gui_window_clearbuffer(&gui_get_windows()[get_current_task_window()], (uint16_t)regs->ebx);

   }

   if(regs->eax == 24) {
      // fat get directory size
      // IN: ebx = directory firstClusterNo
      // OUT: ebx = directory size
      regs->ebx = (uint32_t)fat_get_dir_size(regs->ebx);
   }

   if(regs->eax == 25) {
      // fat get directory size
      // IN: ebx = directory firstClusterNo
      // OUT: ebx
      fat_dir_t *items = malloc(32 * fat_get_dir_size(regs->ebx));
      fat_read_dir((uint16_t)regs->ebx, items);
      regs->ebx = (uint32_t)items;
   }

   if(regs->eax == 26) {
      // print uint ebx to window 0
      gui_window_writeuint(regs->ebx, 0, 0);
      gui_window_writestr("\n", 0, 0);
   }

   if(regs->eax == 27) {
      // override downarrow window function
      uint32_t addr = regs->ebx;

      gui_window_writestr("Overriding downarrow function\n", 0, get_current_task_window());

      gui_get_windows()[get_current_task_window()].downarrow_func = (void *)(addr);
   }

   if(regs->eax == 28) {
      // write num at
      // IN: ebx = num
      // IN: ecx = x
      // IN: edx = y
      gui_window_writenumat(regs->ebx, 0, regs->ecx, regs->edx, get_current_task_window());

   }

}  

void keyboard_handler(registers_t *regs) {
   unsigned char scan_code = inb(0x60);

   if(videomode == 1) gui_interrupt_switchtask(regs);

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
         gui_uparrow(regs);
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
      terminal_writenumat(timer_i, 79);
   } else {
      gui_showtimer(timer_i);

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

         if(int_no == 14) {
            // page error
            uint32_t addr;
	         asm volatile("mov %%cr2, %0" : "=r" (addr));
            gui_window_writestr("Page fault at ", gui_rgb16(255, 100, 100), 0);
            gui_window_writeuint(addr, 0, 0);
            if(page_getphysical(addr) != (uint32_t)-1) {
               gui_window_writestr(" <", gui_rgb16(255, 100, 100), 0);
               gui_window_writeuint(page_getphysical(addr), 0, 0);
               gui_window_writestr(">", gui_rgb16(255, 100, 100), 0);
            }
            gui_window_writestr(" with eip ", gui_rgb16(255, 100, 100), 0);
            gui_window_writeuint(regs->eip, 0, 0);
            if(page_getphysical(regs->eip) != (uint32_t)-1) {
               gui_window_writestr(" <", gui_rgb16(255, 100, 100), 0);
               gui_window_writeuint(page_getphysical(regs->eip), 0, 0);
               gui_window_writestr(">", gui_rgb16(255, 100, 100), 0);
            }
            gui_window_writestr("\n", 0, 0);

            while(true);
         }

         gui_drawrect(gui_rgb16(255, 0, 0), 60, 0, 8*2, 11);
         gui_writenumat(int_no, gui_rgb16(255, 200, 200), 60, 0);

         gui_drawrect(gui_rgb16(255, 0, 0), 200, 0, 8*8, 11);
         gui_writenumat(regs->eip, gui_rgb16(255, 200, 200), 200, 0);
      
         gui_window_writestr("Task ", gui_rgb16(255, 100, 100), 0);
         gui_window_writenum(get_current_task(), 0, 0);
         gui_window_writestr(" ended due to exception ", gui_rgb16(255, 100, 100), 0);
         gui_window_writenum(int_no, 0, 0);
         gui_window_writestr("\n", 0, 0);

         end_task(get_current_task(), regs);
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

         mouse_handler(regs);

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

   gui_window_writestr("Exception ", gui_rgb16(255, 100, 100), 0);
   gui_window_writeuint(int_no, 0, 0);
   gui_window_writestr(" with err code ", gui_rgb16(255, 100, 100), 0);
   gui_window_writeuint(regs->err_code, 0, 0);
   gui_window_writestr(" with eip ", gui_rgb16(255, 100, 100), 0);
   gui_window_writeuint(regs->eip, 0, 0);  
   gui_window_writestr("\n", gui_rgb16(255, 100, 100), 0);

   exception_handler(int_no, regs);
}