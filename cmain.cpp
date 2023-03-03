#include <stdint.h>
#include <stddef.h>

extern "C" {
#include "gui.h"
#include "memory.h"
#include "interrupts.h"
#include "tasks.h"
#include "terminal.h"
#include "registers_t.h"
#include "registers_t.h"

extern bool videomode;

extern void gdt_flush();
extern void tss_flush();
}

void cmain_gui_init() {
   idt_init();
   memory_init();
   gui_init();
   tasks_alloc();
   tss_init();
   gdt_flush();
   tss_flush();

   register_irq(0, *timer_handler);
   register_irq(1, *keyboard_handler);
   register_irq(12, *mouse_handler);
   register_irq(16, *software_handler);
}

void cmain_cli_init() {
   idt_init();
   terminal_clear();
   terminal_prompt();

   register_irq(0, *timer_handler);
   register_irq(1, *keyboard_handler);
}

extern "C" {

void cmain() {
   if(videomode)
      cmain_gui_init();
   else
      cmain_cli_init();
}

}