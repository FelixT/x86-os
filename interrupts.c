// https://wiki.osdev.org/Interrupts_tutorial
// https://wiki.osdev.org/Interrupt_Descriptor_Table

#include <stdint.h>

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
static inline void outb(uint16_t port, uint8_t val)
{
   asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
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
 
void idt_init() {
   idtr.base = (uintptr_t)&idt[0];
   idtr.limit = (uint16_t)sizeof(idt_entry_t) * 256 - 1;
 
   for (uint8_t vector = 0; vector < 32; vector++) {
      idt_set_descriptor(vector, isr_stub_table[vector], 0x8E);
   }
 
   __asm__ volatile("lidt %0" : : "m"(idtr)); // load idt

   // mask all interrupts except keyboard
   // outb(0x21,0xfd); // master data
   // outb(0xa1,0xff); // slave data

   __asm__ volatile("sti"); // set the interrupt flag (enable interrupts)
}

extern void terminal_clear(void);
extern void terminal_write(char* str);
extern void terminal_writenumat(int num, int at);

void exception_handler(int irq) {

   terminal_clear();

   terminal_write("INT");

   unsigned char scan_code = inb(0x60);
   char* c = "x\0";
   c[0] = scan_code;
   terminal_writenumat(irq, 0);

   // send end of command code 0x20 to pic
   if(irq >= 8)
      outb(0xA0, 0x20); // slave command

   outb(0x20, 0x20); // master command
}