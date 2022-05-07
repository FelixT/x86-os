// https://wiki.osdev.org/Interrupts_tutorial
// https://wiki.osdev.org/Interrupt_Descriptor_Table

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

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

   for (uint8_t vector = 32; vector < 47; vector++) {
      idt_set_descriptor(vector, isr_stub_table[vector], 0x8E);
   }
 
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

char scan_to_char(int scan_code) {
   // https://www.millisecond.com/support/docs/current/html/language/scancodes.htm

   char upperFirst[13] = "1234567890-=";
   char upperSecond[13] = "QWERTYUIOP[]";
   char upperThird[13] = "ASDFGHJKL;'#";
   char upperFourth[12] = "\\ZXCVBNM,./";

   if(scan_code >= 1 && scan_code <= 13)
      return upperFirst[scan_code-1];

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
#define command_maxlen 40
char command_buffer[command_maxlen+1];
int command_index = 0;


int strlen(char* str) {
   int len = 0;
   while(str[len] != '\0')
      len++;
   return len;
}

bool strcmp(char* str1, char* str2) {
   int len = strlen(str1);
   if(len != strlen(str2))
      return false;

   for(int i = 0; i < len; i++)
      if(str1[i] != str2[i])
         return false;

   return true;
}

void check_cmd(char* command) {
   if(strcmp(command, "WICKED")) {
      terminal_write("\nyep, wicked\n");
   }

   if(strcmp(command, "CLEAR")) {
      terminal_clear();
   }
}

void exception_handler(int int_no) {

   if(int_no < 32) {
      
   } else {
      // IRQ numbers: https://www.computerhope.com/jargon/i/irq.htm

      int irq_no = int_no - 32;

      if(irq_no == 0) {
         // system timer, do nothing

         terminal_writenumat(timer_i++, 79);
         timer_i%=10;
      }

      if(irq_no == 1) {
         // keyboard

         unsigned char scan_code = inb(0x60);

         if(scan_code == 28)  { // return
            terminal_write("\n");

            command_buffer[command_index] = '\0';
            terminal_write(command_buffer);
            check_cmd(command_buffer);

            command_index = 0;

            terminal_prompt();

         } else if(scan_code == 14) { // backspace
            
            if(command_index > 0) {
               command_index--;
               terminal_backspace();
            }

         } else {

            if(command_index < command_maxlen) {
               char letter[2] = "x";
               letter[0] = scan_to_char(scan_code);
               terminal_write(letter);

               if(letter[0] != '\0') {
                  command_buffer[command_index++] = letter[0];
               }
            } else {} // no more space in buffer
         }
      }

   }

   ///terminal_clear();

   terminal_writeat("  ", 0);
   terminal_writenumat(int_no, 0);

   // send end of command code 0x20 to pic
   if(int_no >= 8)
      outb(0xA0, 0x20); // slave command

   outb(0x20, 0x20); // master command
}