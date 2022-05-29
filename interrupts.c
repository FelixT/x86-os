// https://wiki.osdev.org/Interrupts_tutorial
// https://wiki.osdev.org/Interrupt_Descriptor_Table

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "tasks.c"

typedef struct {
   uint16_t    isr_low;      // lower 16 bits of isr address/offset
   uint16_t    kernel_cs;    // code segment in gdt, the CPU will load this into cs before calling the isr
   uint8_t     zero;
   uint8_t     attributes;
   uint16_t    isr_high;     // higher 16 bits of isr address/offset
} __attribute__((packed)) idt_entry_t;

__attribute__((aligned(0x10))) 
static idt_entry_t idt[256];

typedef struct {
   uint16_t	limit;
   uint32_t	base;
} __attribute__((packed)) idtr_t;

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
 
   for (uint8_t vector = 0; vector < 32; vector++) {
      idt_set_descriptor(vector, isr_stub_table[vector], 0x8E);
   }

   for (uint8_t vector = 32; vector < 49; vector++) {
      idt_set_descriptor(vector, irq_stub_table[vector-32], 0x8E);
   }

   //idt_set_descriptor(0x80, irq_stub_table[32], 0x8E);
 
   __asm__ volatile("lidt %0" : : "m"(idtr)); // load idt

   // mask all interrupts except keyboard
   // outb(0x21,0xfd); // master data
   // outb(0xa1,0xff); // slave data

   pic_remap();

   __asm__ volatile("sti"); // set the interrupt flag (enable interrupts)

}

extern void terminal_clear(void);
extern void terminal_write(char* str);
extern void terminal_writeat(char* str, int at);
extern void terminal_writenumat(int num, int at);
extern void terminal_backspace(void);
extern void terminal_prompt(void);

extern void gui_draw(void);
extern int videomode;

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

int timer_i = 0;

extern void gui_clear(int colour);
extern void gui_drawchar(char c, int colour);
extern void gui_writenumat(int num, int colour, int x, int y);
extern void gui_writenum(int num, int colour);
extern void gui_writestr(char *c, int colour);
extern void gui_drawrect(uint8_t colour, int x, int y, int width, int height);
extern void gui_keypress(char key);
extern void gui_return(void *regs);
extern void gui_backspace();
extern void gui_writestrat(char *c, int colour, int x, int y);

extern void terminal_keypress(char key);
extern void terminal_return();

extern void mouse_update(uint32_t relX, uint32_t relY);
extern void mouse_leftclick(int relX, int relY);
extern void mouse_leftrelease();

int mouse_cycle = 0;
uint8_t mouse_data[3];

void exception_handler(int int_no, registers_t *regs) {

   if(int_no < 32) {

      if(videomode == 1) {
         gui_drawrect(4, 60, 0, 6*2, 7);
         gui_writenumat(int_no, 14, 60, 0);
      }
         
   } else {
      // IRQ numbers: https://www.computerhope.com/jargon/i/irq.htm

      int irq_no = int_no - 32;

      if(irq_no == 0) {
         // system timer
         
         if(videomode == 0) {
            terminal_writenumat(timer_i, 79);
         } else {
            gui_drawrect(3, 314, 0, 5, 7);
            gui_writenumat(timer_i, 7, 314, 0);
         }

         timer_i++;
         timer_i%=10;
         
      } else if(irq_no == 1) {
         // keyboard

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

         } else {
            // other key pressed

            char c = scan_to_char(scan_code);

            if(videomode == 1)
               gui_keypress(c);
            else
               terminal_keypress(c);
         }
      } else if(irq_no == 12) {

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

      } else if(irq_no == 16) {

         // 0x30: software interrupt
         // uses eax, ebx, ecx as potential arguments
         // note: edx is used in our handler to store the pre-interrupt esp

         gui_writenum(regs->eax, 0);

         if(regs->eax == 1) {
            // WRITE STRING...
            // ebx contains string address
            gui_writestr((char*)regs->ebx, 0);
         }

         if(regs->eax == 2) {
            // WRITE NUMBER...
            // ebx contains int
            gui_writenum(regs->ebx, 0);
            gui_writenumat(regs->ebx, 14, 60, 20);
         }

         if(regs->eax == 3) {
            // yield

            // save registers
            tasks[current_task].registers = *regs;
            tasks[current_task].active = false;

            current_task++;
            current_task%=TOTAL_TASKS;
            // restore registers
            *regs = tasks[current_task].registers;
            tasks[current_task].active = true;
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
            // launch task ebx
            current_task = regs->ebx;
            tasks[current_task].registers.esp = regs->esp;//temporary
            tasks[current_task].registers.cs = regs->cs;
            tasks[current_task].registers.eflags = regs->eflags;
            tasks[current_task].registers.useresp = regs->useresp;
            tasks[current_task].registers.ss = regs->ss;

            tasks[1].registers.esp = regs->esp;//temporary
            tasks[1].registers.cs = regs->cs;
            tasks[1].registers.eflags = regs->eflags;
            tasks[1].registers.useresp = regs->useresp;
            tasks[1].registers.ss = regs->ss;

            *regs = tasks[current_task].registers;
            tasks[current_task].active = true;
         }
         

      } else {

         // other interrupt no
         if(videomode == 0) {
            terminal_writeat("  ", 0);
            terminal_writenumat(int_no, 0);
         } else {
            gui_drawrect(3, 0, 0, 7*2, 7);
            gui_writenumat(int_no, 0, 0, 0);
         }

      }

      // send end of command code 0x20 to pic
      if(irq_no >= 8)
         outb(0xA0, 0x20); // slave command

      outb(0x20, 0x20); // master command

   }
}

void err_exception_handler(int int_no, int error_code, registers_t *regs) {
   //uint32_t *zer = (uint32_t*) 0x00;

   //zer[0] = int_no;
   //zer[1] = error_code;
   //gui_writenumat(int_no, 0, 30, 100);

   gui_writenum(error_code, 0);
   gui_writestr(" ", 0);

   exception_handler(int_no, regs);
}