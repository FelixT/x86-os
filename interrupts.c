// https://wiki.osdev.org/Interrupts_tutorial
// https://wiki.osdev.org/Interrupt_Descriptor_Table

#include "interrupts.h"
#include "window.h"
#include "api.h"
#include "events.h"
#include "ata.h"
#include "windowmgr.h"
#include "memory.h"
#include "window_popup.h"
#include "tasks.h"
#include "cpu.h"

extern void* isr_stub_table[];
extern void* irq_stub_table[];

extern int videomode;
extern bool switching; // preemptive multitasking enabled

__attribute__((aligned(0x10))) 
static idt_entry_t idt[256];

static idtr_t idtr;

#define IRQ_SIZE 32
void (*irqs[IRQ_SIZE])(registers_t *regs);

#define DEBUG_DELAYS 0

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
      idt_set_descriptor(vector, isr_stub_table[vector], 0x8E);

   for(uint8_t vector = 32; vector < 49; vector++)
      idt_set_descriptor(vector, irq_stub_table[vector-32], 0x8E);

   for(uint8_t i = 0; i < IRQ_SIZE; i++)
      irqs[i] = NULL;

   idt_set_descriptor(48, irq_stub_table[48-32], 0xEE); // software interrupt 0x30, can be called from ring 3
 
   // double fault (exception 8) task gate, switch to df tss (using df kstack)
   idt[8].isr_low = 0;
   idt[8].isr_high = 0;
   idt[8].kernel_cs = TSU_DF_SEG;
   idt[8].attributes = 0x85; // task gate

   __asm__ volatile("lidt %0" : : "m"(idtr)); // load idt

   pic_remap();

   __asm__ volatile("sti"); // set the interrupt flag (enable interrupts)

}

char scan_to_char(int scan_code, bool caps) {
   // https://www.millisecond.com/support/docs/current/html/language/scancodes.htm

   char upperFirst[13] = "1234567890-=";
   char upperSecond[13] = "QWERTYUIOP[]";
   char upperThird[13] = "ASDFGHJKL;'#";
   char upperFourth[12] = "\\ZXCVBNM,./";

   char shiftFirst[13] = "!@#$%^&*()_+";
   char shiftSecond[13] = "QWERTYUIOP{}";
   char shiftThird[13] = "ASDFGHJKL:\"~";
   char shiftFourth[12] = "|ZXCVBNM<>?";

   char c = '\0';

   if(scan_code >= 2 && scan_code <= 13) {
      if(caps) {
         c = shiftFirst[scan_code-2];
      } else {
         c = upperFirst[scan_code-2];
      }
   }
   if(scan_code >= 16 && scan_code <= 27) {
      if(caps) {
         c = shiftSecond[scan_code-16];
      } else {
         c = upperSecond[scan_code-16];
      }
   }
   if(scan_code >= 30 && scan_code <= 41) {
      if(caps) {
         c = shiftThird[scan_code-30];
      } else {
         c = upperThird[scan_code-30];
      }
   }
   if(scan_code >= 43 && scan_code <= 53) {
      if(caps) {
         c = shiftFourth[scan_code-43];
      } else {
         c = upperFourth[scan_code-43];
      }
   }
   if(scan_code == 57)
      c = ' ';

   if(!caps && c >= 'A' && c <= 'Z')
      c += ('a' - 'A');

   return c;
}

void register_irq(int index, void (*handler)(registers_t *regs)) {
   irqs[index] = handler;
}

void software_handler(registers_t *regs) {

   task_state_t *task = &gettasks()[get_current_task()];
   task->in_syscall = true;
   task->syscall_no = regs->eax;

   switch(regs->eax) {
      case 1:
         api_write_string(regs);
         break;
      case 2:
         api_write_number(regs);
         break;
      case 3:
         api_yield(regs);
         break;
      case 4:
         api_print_program_stack(regs);
         break;
      case 5:
         api_print_stack();
         break;
      case 6:
         api_write_uint(regs);
         break;
      case 7:
         api_return_framebuffer(regs);
         break;
      case 8:
         api_write_newline();
         break;
      case 9:
         api_redraw_window(regs);
         break;
      case 10:
         api_end_task(regs);
         break;
      case 11:
         api_override_hover(regs);
         break;
      case 12:
         api_end_subroutine(regs);
         break;
      case 13:
         api_override_mouseclick(regs);
         break;
      case 14:
         api_return_window_width(regs);
         break;
      case 15:
         api_return_window_height(regs);
         break;
      case 16:
         //api_malloc(regs); // replaced with dma
         break;
      case 17:
         api_override_rightclick(regs);
         break;
      case 18:
         api_set_window_setting(regs);
         break;
      case 19:
         api_get_window_setting(regs);
         break;
      case 20:
         api_set_window_position(regs);
         break;
      case 21:
         api_draw_bmp(regs);
         break;
      case 22:
         api_write_string_at(regs);
         break;
      case 23:
         api_clear_window(regs);
         break;
      case 24:
         api_set_window_minimised(regs);
         break;
      case 25:
         api_read_dir(regs);
         break;
      case 26:
         api_get_window_position(regs);
         break;
      case 27: 
         api_override_close(regs);
         break;
      case 28:
         api_write_number_at(regs);
         break;
      case 29:
         api_override_draw(regs);
         break;
      case 30:
         api_queue_event(regs);
         break;
      case 31:
         api_sleep(regs);
         break;
      case 32:
         api_launch_task(regs);
         break;
      case 33:
         api_get_setting(regs);
         break;
      case 34:
         api_override_resize(regs);
         break;
      case 35:
         api_set_setting(regs);
         break;
      case 36:
         api_override_drag(regs);
         break;
      case 37:
         api_redraw_region(regs);
         break;
      case 38:
         api_override_release(regs);
         break;
      case 39:
         api_override_checkcmd(regs);
         break;
      case 40:
         //api_free(regs);
         break;
      case 41:
         api_new_file(regs);
         break;
      case 42:
         api_set_window_title(regs);
         break;
      case 43:
         api_set_working_dir(regs);
         break;
      case 44:
         api_get_working_dir(regs);
         break;
      case 45:
         api_override_mouseout(regs);
         break;
      case 46:
         api_override_keyrelease(regs);
         break;
      case 47:
         api_read(regs);
         break;
      case 48:
         api_seek(regs);
         break;
      case 49:
         api_debug_write_str(regs);
         break;
      case 50:
         api_mkdir(regs);
         break;
      case 51:
         api_sbrk(regs);
         break;
      case 52:
         api_open(regs);
         break;
      case 53:
         api_write(regs);
         break;
      case 54:
         api_create_scrollbar(regs);
         break;
      case 55:
         api_set_scrollable_height(regs);
         break;
      case 56:
         api_scroll_to(regs);
         break;
      case 57:
         api_get_timer_tick(regs);
         break;
      case 58:
         api_rename(regs);
         break;
      case 59:
         api_fsize(regs);
         break;
      case 60:
         api_set_window_size(regs);
         break;
      case 61:
         api_get_font_info(regs);
         break;
      case 62:
         api_create_window(regs);
         break;
      case 63:
         api_close_window(regs);
         break;
      case 64:
         api_override_keypress(regs);
         break;
      case 65:
         api_create_thread(regs);
         break;
      case 66:
         api_get_tasks(regs);
         break;
      case 67:
         api_kill_task(regs);
         break;
      case 68:
         api_unlink(regs);
         break;
      case 69:
         api_rmdir(regs);
         break;
      case 70:
         api_pipe(regs);
         break;
      case 71:
         api_unpause(regs);
         break;
      case 72:
         api_dup2(regs);
         break;
      case 73:
         api_close(regs);
         break;
      case 74:
         api_dup(regs);
         break;
      case 75:
         api_get_time(regs);
         break;
      case 76:
         api_futex_wait(regs);
         break;
      case 77:
         api_futex_wake(regs);
         break;
      case 78:
         api_shared_create(regs);
         break;
      case 79:
         api_shared_grant(regs);
         break;
      case 80:
         api_shared_map(regs);
         break;
      case 81:
         api_shared_close(regs);
         break;
      case 82:
         api_pci_map(regs);
         break;
      case 83:
         api_pci_exists(regs);
         break;
      case 84:
         api_dma(regs);
         break;
      case 85:
         api_close_handle(regs);
         break;
      case 86:
         api_escalate(regs);
         break;
      case 87:
         api_fpsize(regs);
         break;
      case 88:
         api_truncate(regs);
         break;
      case 89:
         api_create_port(regs);
         break;
      case 90:
         api_port_connect(regs);
         break;
      case 91:
         api_override_msg(regs);
         break;
      case 92:
         api_msg_send(regs);
         break;
      case 93:
         api_msg_read(regs);
         break;
      case 94:
         api_wait_on_receive(regs);
         break;
      case 95:
         api_snooze(regs);
         break;
      case 96:
         api_port_close(regs);
         break;
      case 97:
         api_port_disconnect(regs);
         break;
      case 98:
         api_msg_call(regs);
         break;
      case 99:
         api_msg_receive(regs);
         break;
      case 100:
         api_msg_reply(regs);
         break;
      default:
         debug_printf("Unknown syscall %i\n", regs->eax);
         break;
   }

   task->in_syscall = false;
   if(task->kill_pending) {
      debug_printf("Killed after syscall %i\n", task->syscall_no);
      end_task(task->task_id, regs);
   }
}

void keyboard_handler(registers_t *regs) {
   unsigned char scan_code = inb(0x60);

   if(videomode == 0) {

      if(scan_code == 28)
         terminal_return();
      else if(scan_code == 14)
         terminal_backspace();
      else
         terminal_keypress(scan_to_char(scan_code, true));

   } else {
      gui_keypress(scan_code);
      wm_event_thread_exit_resume(regs);
   }

}

int mouse_cycle = 0;
uint8_t mouse_data[4];

extern bool mouse_enabled;
extern bool mouse_scrolling_enabled;

void mouse_handler(registers_t *regs) {
   if(!mouse_enabled) return;

   uint8_t status = inb(0x64);
   if(!(status & 0x01) || !(status & 0x20)) return;

   uint8_t data = inb(0x60);

   if(mouse_cycle == 0 && !(data & 0x08))
      return; // wait for sync

   int bytes = mouse_scrolling_enabled ? 4 : 3;

   mouse_data[mouse_cycle] = data;
   mouse_cycle++;

   if(mouse_cycle == bytes) {
      int relX = mouse_data[1];
      int relY = mouse_data[2];

      // handle case of negative relative values
      if(mouse_data[0] & 0x10) relX -= 256;
      if(mouse_data[0] & 0x20) relY -= 256;

      mouse_update(&relX, &relY); // applies scaling

      if(mouse_scrolling_enabled) {
         int scroll = (int8_t)mouse_data[3];
         if(scroll == -1) {
            // scroll up
            wm_event(MOUSE_SCROLL, -SCROLL_AMOUNT, -1, 0);
         } else if(scroll == 1) {
            // scroll down
            wm_event(MOUSE_SCROLL, SCROLL_AMOUNT, -1, 0);
         }
      }

      if(mouse_data[0] & 0x2)
         mouse_rightclick();
      else if(mouse_data[0] & 0x1)
         mouse_leftclick(relX, relY);
      else
         mouse_release();
      
      if(relX != 0 || relY != 0) {
         gui_cursor_save_bg();
         gui_cursor_draw();
      }

      mouse_cycle = 0;

      wm_event_thread_exit_resume(regs);
   }
}

uint32_t timer_i = 0;
int timer_hz;

void timer_set_hz(int hz) {
   timer_hz = hz;

   // set pit to ~new hz
   int divisor = 1193180 / hz;
    
   asm volatile (
      "movb $0x36, %%al\n\t"
      "outb %%al, $0x43\n\t"      
      
      "movl %0, %%eax\n\t"
      "outb %%al, $0x40\n\t"
      "shrl $8, %%eax\n\t"
      "outb %%al, $0x40"
      :
      : "r"(divisor)              
      : "eax"
   );
}

#if DEBUG_DELAYS
uint32_t delayed_ticks = 0;
#endif

void timer_handler(registers_t *regs) {
   timer_i++;
   events_check(regs);

#if DEBUG_DELAYS
   outb(0x20, 0x0A); // check pic irr bit 0 for unresolved
   if(inb(0x20) & 1)
      delayed_ticks++;

   if(timer_i % 1000 == 0 && delayed_ticks) {
      debug_printf("missed %u/1000 ticks\n", delayed_ticks);
      delayed_ticks = 0;
   }
#endif

   if(videomode == 0) {
      terminal_writenumat(timer_i%10, 79);
   } else {
      if(timer_i%340 == 0) { // ~2fps auto redraw
         gui_draw();
      }

      if(timer_i%3 == 0 && (regs->cs & 3) != 0) {
         // don't preempt when interrupting kernel

         // note: immediately swaps if new task was parked in kernel
         switch_task(regs, true);
      }
   }
}

uint32_t get_timer_tick() {
   return timer_i;
}

void crash_task(int int_no, registers_t *regs, int task) {
   pause_task(task, regs); // pauses and marks and crashed
   uint32_t process_uid = gettasks()[task].process->uid;
   wm_event(TASK_CRASH, task, process_uid, (uint16_t)int_no);
}

extern int current_servicing_task;

bool page_fault_handler(registers_t *regs) {
   uint32_t addr = read_cr2();

   // page error
   task_state_t *task = get_current_task_state();
   if(current_servicing_task != -1) {
      task = &gettasks()[current_servicing_task];
   }
   process_t *process = task->process;
   if(!process) return false;
   page_dir_entry_t *dir = task->process->page_dir;

   // demand paging/lazy allocation: check if within heap
   if(dir != page_get_current()) {
      debug_printf("int 14 - task page directory isn't current...\n");
   }

   if(task_addr_demand_paged(task, addr)) // task_addr_demand_paged()
      return task_demand_map(process, addr);

   // not within heap, show error and pause task

   window_writestr("Page fault at ", gui_rgb16(255, 100, 100), 0);
   debug_printf("0x%h", addr);
   if(page_getphysical(dir, addr) != (uint32_t)-1) {
      window_writestr(" <", gui_rgb16(255, 100, 100), 0);
      debug_writehex(page_getphysical(dir, addr));
      window_writestr(">", gui_rgb16(255, 100, 100), 0);
   }
   window_writestr(" with eip ", gui_rgb16(255, 100, 100), 0);
   debug_writehex(regs->eip);
   if(page_getphysical(dir, regs->eip) != (uint32_t)-1) {
      window_writestr(" <", gui_rgb16(255, 100, 100), 0);
      debug_writehex(page_getphysical(dir, regs->eip));
      window_writestr(">", gui_rgb16(255, 100, 100), 0);
   }

   window_writestr(" ebp ", gui_rgb16(255, 100, 100), 0);
   debug_writehex(regs->ebp);
   if(page_getphysical(dir, regs->ebp) != (uint32_t)-1) {
      window_writestr(" <", gui_rgb16(255, 100, 100), 0);
      debug_writehex(page_getphysical(dir, regs->ebp));
      window_writestr(">", gui_rgb16(255, 100, 100), 0);
   }

   window_writestr(" useresp ", gui_rgb16(255, 100, 100), 0);
   debug_writehex(regs->useresp);
   if(page_getphysical(dir, regs->useresp) != (uint32_t)-1) {
      window_writestr(" <", gui_rgb16(255, 100, 100), 0);
      debug_writehex(page_getphysical(dir, regs->useresp));
      window_writestr(">", gui_rgb16(255, 100, 100), 0);
   }

   window_writestr(" and esp ", gui_rgb16(255, 100, 100), 0);
   debug_writehex(regs->esp);
   if(page_getphysical(dir, regs->esp) != (uint32_t)-1) {
      window_writestr(" <", gui_rgb16(255, 100, 100), 0);
      debug_writehex(page_getphysical(dir, regs->esp));
      window_writestr(">", gui_rgb16(255, 100, 100), 0);
   }

   debug_printf("\nHeap 0x%h - 0x%h\n", process->heap_start, process->heap_end);
   debug_printf("Stack 0x%h - 0x%h\n", task->v_stack_start+0x1000, task->v_stack_start+TASK_STACK_SIZE);

   return false;
}

typedef enum {
   FAULT_USER,
   FAULT_SYSCALL,
   FAULT_KTHREAD, // includes binaries as these run with kernel pagedir
   FAULT_SWITCHING, // kernel code after task switch
   FAULT_KERNEL // i.e. irq
} fault_type_t;

fault_type_t identify_fault_type(registers_t *regs) {
   if(regs->cs & 3)
      return FAULT_USER;
   task_state_t *task = get_current_task_state();
   bool own_kstack = (uint32_t)regs > task->kernel_stack_top - KSTACK_SIZE && (uint32_t)regs <= task->kernel_stack_top;
   if(!own_kstack) return FAULT_SWITCHING;
   if(task->in_syscall && !task->process)
      return FAULT_KERNEL;
   if(task->in_syscall && task->process->page_dir == page_get_kernel_pagedir())
      return FAULT_KTHREAD;
   if(task->in_syscall)
      return FAULT_SYSCALL;
   return FAULT_KERNEL;
}

void err_exception_handler(int int_no, registers_t *regs);

void exception_handler(int int_no, registers_t *regs) {
   int irq_no = int_no - 32;
   if(irq_no < 0) {
      err_exception_handler(int_no, regs);
      return;
   }

   // check for spurious irqs
   if(irq_no == 7) {
      outb(0x20, 0x0B); // read master isr
      uint8_t isr = inb(0x20);
      if(!((isr>>7)&0x1)) {
         debug_printf("spurious irq 7\n");
         return; // no handling
      }
   } else if(irq_no == 15) {
      outb(0xA0, 0x0B); // read slave isr
      uint8_t isr = inb(0xA0);
      if(!((isr>>7)&0x1)) {
         debug_printf("spurious irq 15\n");
         outb(0x20, 0x20); // send eoi to master
         return;
      }
   }

   // send end of command code 0x20 to pic
   // safe to do this here as interrupts are disabled
   if(int_no < 48) {
      if(int_no >= 40) {
         outb(0xA0, 0x20); // slave command
      }
      outb(0x20, 0x20); // master command
   }

   // IRQ numbers: https://www.computerhope.com/jargon/i/irq.htm

   if(irq_no >= IRQ_SIZE || irqs[irq_no] == NULL) {
      // unhandled interrupt, print it
      char buffer[256];
      sprintf(buffer, "Unhandled interrupt %i\n", irq_no);
      if(videomode == 0) {
         terminal_write(buffer);
      } else {
         gui_drawrect(COLOUR_CYAN, 0, 0, 7*2, 7);
         gui_writenumat(int_no, 0, 0, 0);
         debug_writestr(buffer);
      }
   } else {
      irqs[irq_no](regs);
   }

   if((regs->cs & 3) && get_current_task() >= 0) {
      if(!task_execute_queued_subroutine(regs, get_current_task()) && get_current_task_state()->crashed)
         switch_task(regs, false);
   }
}

bool panic = false;
__attribute__((noreturn)) void kernel_panic(void) {
   // show debug window and panic
   if(panic) {
      while(true)
         asm("cli; hlt");
   }
   panic = true;
   switching = false; // prevents kernel yield
   setSelectedWindowIndex(0);
   gui_window_t *window = getSelectedWindow();
   window->minimised = false;
   window->needs_redraw = true;
   window_draw(window);
   int popup = windowmgr_add();
   if(popup < 0)
      while(true) {};
   gui_window_t *popup_window = getWindow(popup);
   window_popup_dialog(popup_window, NULL, "f3sys has crashed");
   popup_window->x = 5;
   popup_window->y = 5;
   strcpy(popup_window->title, "kernel panic");
   popup_window->window_objects[1]->visible = false; // ok btn
   popup_window->window_objects[0]->y += 14; // txt
   extern surface_t surface;
   extern surface_t main_surface;
   surface = main_surface;
   windowmgr_redrawall();
   while(true)
      asm("cli; hlt");
}

void err_exception_handler(int int_no, registers_t *regs) {
   if(get_current_task() < 0) { // early in boot
      debug_printf("Exception %i with task %i\n", int_no, get_current_task());
      kernel_panic();
      return;
   }
   if(videomode == 0) return; // cli

   // https://wiki.osdev.org/Exceptions

   if(int_no == 14) {
      if(page_fault_handler(regs)) {
         return; // demand paging
      }
   }

   window_writestr("Exception ", gui_rgb16(255, 100, 100), 0);
   window_writeuint(int_no, 0, 0);
   window_writestr(" with err code ", gui_rgb16(255, 100, 100), 0);
   window_writeuint(regs->err_code, 0, 0);
   window_writestr(" with eip ", gui_rgb16(255, 100, 100), 0);
   debug_writehex(regs->eip);  
   window_writestr("\n", gui_rgb16(255, 100, 100), 0);

   char buffer[200];
   gui_drawrect(gui_rgb16(180, 0, 0), 60, 0, 8*2, 11);
   gui_writenumat(int_no, gui_rgb16(255, 200, 200), 62, 2);

   sprintf(buffer, "0x%h", regs->eip);
   gui_drawrect(gui_rgb16(180, 0, 0), 120, 0, 8*8, 11);
   gui_writestrat(buffer, gui_rgb16(255, 200, 200), 122, 2);

   if(int_no == 14)
      debug_printf("Page fault at address 0x%h\n", read_cr2());

   fault_type_t fault_type = identify_fault_type(regs);
   if(fault_type == FAULT_USER) {
      int task = get_current_task();
      task_state_t *task_state = get_current_task_state();
      window_writestr("Task ", gui_rgb16(255, 100, 100), 0);
      window_writenum(task, 0, 0);
      window_writestr(" paused due to exception ", gui_rgb16(255, 100, 100), 0);
      window_writenum(int_no, 0, 0);
      window_writestr("\n", 0, 0);
      
      if(task_state->in_routine)
         debug_printf("Task was in routine %s\n", task_state->routine_name);

      crash_task(int_no, regs, task);
   } else {
      window_writestr("Fault type ", gui_rgb16(255, 100, 100), 0);
      window_writenum(fault_type, 0, 0);
      if(fault_type == FAULT_SYSCALL)
         debug_printf(" - syscall");
      if(fault_type == FAULT_KERNEL)
         debug_printf(" - kernel");
      if(fault_type == FAULT_SWITCHING)
         debug_printf(" - switching");
      if(fault_type == FAULT_KTHREAD)
         debug_printf(" - kthread [%s]", get_current_task_state()->process->exe_path);
      window_writestr(" (FATAL)\n", gui_rgb16(255, 100, 100), 0);

      debug_printf("current_task: %i\n", get_current_task());
      if(current_servicing_task != -1)
         debug_printf("Exception occurred while servicing task %i\n", current_servicing_task);

      kernel_panic(); // any exception in the kernel panics
   }

   if((regs->cs & 3) && get_current_task() >= 0) {
      if(!task_execute_queued_subroutine(regs, get_current_task()) && get_current_task_state()->crashed)
         switch_task(regs, false);
   }
}
