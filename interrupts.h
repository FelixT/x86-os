#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "tasks.h"
#include "gui.h"

typedef struct {
   uint16_t    isr_low;      // lower 16 bits of isr address/offset
   uint16_t    kernel_cs;    // code segment in gdt, the CPU will load this into cs before calling the isr
   uint8_t     zero;
   uint8_t     attributes;
   uint16_t    isr_high;     // higher 16 bits of isr address/offset
} __attribute__((packed)) idt_entry_t;

typedef struct {
   uint16_t	limit;
   uint32_t	base;
} __attribute__((packed)) idtr_t;

typedef struct {
   uint16_t limit_low;
	uint16_t base_low;
	uint8_t base_middle;
	uint8_t access;
	uint8_t granularity;
	uint8_t base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
	uint32_t prev_tss;
	uint32_t esp0;
	uint32_t ss0;
	uint32_t esp1;
	uint32_t ss1;
	uint32_t esp2;
	uint32_t ss2;
	uint32_t cr3;
	uint32_t eip;
	uint32_t eflags;
	uint32_t eax;
	uint32_t ecx;
	uint32_t edx;
	uint32_t ebx;
	uint32_t esp;
	uint32_t ebp;
	uint32_t esi;
	uint32_t edi;
	uint32_t es;
	uint32_t cs;
	uint32_t ss;
	uint32_t ds;
	uint32_t fs;
	uint32_t gs;
	uint32_t ldt;
	uint16_t trap;
	uint16_t iomap_base;
} __attribute__((packed)) tss_t;

extern void terminal_keypress(char key);
extern void terminal_return();

extern void terminal_clear(void);
extern void terminal_write(char* str);
extern void terminal_writeat(char* str, int at);
extern void terminal_writenumat(int num, int at);
extern void terminal_backspace(void);
extern void terminal_prompt(void);


extern void mouse_update(uint32_t relX, uint32_t relY);
extern void mouse_leftclick(registers_t *regs, int relX, int relY);
extern void mouse_leftrelease();