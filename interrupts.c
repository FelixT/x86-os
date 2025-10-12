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

extern void* isr_stub_table[];
extern void* irq_stub_table[];
extern void* software_interrupt_routine;

extern int videomode;
extern bool switching; // preemptive multitasking enabled
bool switching_paused = false;

extern void mouse_update(uint32_t relX, uint32_t relY);
extern void mouse_leftclick(registers_t *regs, int relX, int relY);
extern void mouse_rightclick(registers_t *regs);
extern void mouse_release(registers_t *regs);

__attribute__((aligned(0x10))) 
static idt_entry_t idt[256];

static idtr_t idtr;

void (*irqs[32])(registers_t *regs);

registers_t *cur_regs = NULL;

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

   if(!caps && !caps && c >= 'A' && c <= 'Z')
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
         api_print_stack(regs);
         break;
      case 6:
         api_write_uint(regs);
         break;
      case 7:
         api_return_framebuffer(regs);
         break;
      case 8:
         api_write_newline(regs);
         break;
      case 9:
         api_redraw_window();
         break;
      case 10:
         api_end_task(regs);
         break;
      case 11:
         api_override_uparrow(regs);
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
         api_malloc(regs);
         break;
      //case 17:
      //   api_fat_get_bpb(regs);
      //   break;
      //case 18:
      //   api_fat_read_root(regs);
      //   break;
      //case 19:
      //   api_fat_parse_path(regs);
      //   break;
      //case 20:
      //   api_fat_read_file(regs);
      //   break;
      case 21:
         api_draw_bmp(regs);
         break;
      case 22:
         api_write_string_at(regs);
         break;
      case 23:
         api_clear_window(regs);
         break;
      //case 24:
      //   api_get_get_dir_size(regs);
      //   break;
      case 25:
         api_read_dir(regs);
         break;
      //case 26:
      //   api_get_get_dir_size(regs);
      //   break;
      case 27: 
         api_override_downarrow(regs);
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
         api_register_windowobj(regs);
         break;
      case 32:
         api_launch_task(regs);
         break;
      //case 33:
      //   api_fat_write_file(regs);
      //   break;
      case 34:
         api_override_resize(regs);
         break;
      case 35:
         api_set_sys_font(regs);
         break;
      case 36:
         api_override_drag(regs);
         break;
      case 37:
         api_redraw_pixel(regs);
         break;
      case 38:
         api_override_mouserelease(regs);
         break;
      case 39:
         api_override_checkcmd(regs);
         break;
      case 40:
         api_free(regs);
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
         api_display_popup(regs);
         break;
      case 46:
         api_display_colourpicker(regs);
         break;
      case 47:
         api_read(regs);
         break;
      case 48:
         api_display_filepicker(regs);
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
         api_windowobj_add_child(regs);
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
      default:
         debug_printf("Unknown syscall %i\n", regs->eax);
         break;
   }

   task->in_syscall = false;
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
      gui_keypress(regs, scan_code);
   }

}

int mouse_cycle = 0;
uint8_t mouse_data[3];

extern bool mouse_enabled;
void mouse_handler(registers_t *regs) {
   if(!mouse_enabled) return;

   uint8_t status = inb(0x64);
   if(!(status & 0x01) || !(status & 0x20)) return;

   uint8_t data = inb(0x60);

   if(mouse_cycle == 0 && !(data & 0x08))
      return; // wait for sync

   mouse_data[mouse_cycle] = data;
   mouse_cycle++;

   if(mouse_cycle == 3) {
      int relX = mouse_data[1];
      int relY = mouse_data[2];

      // handle case of negative relative values
      if(mouse_data[0] & 0x10) relX -= 256;
      if(mouse_data[0] & 0x20) relY -= 256;

      mouse_update(relX, relY);

      if(regs) {
         if(mouse_data[0] & 0x2)
            mouse_rightclick(regs);
         else if(mouse_data[0] & 0x1)
            mouse_leftclick(regs, relX, relY);
         else
            mouse_release(regs);
      }
      
      if(relX != 0 || relY != 0) {
         gui_cursor_save_bg();
         gui_cursor_draw();
      }

      mouse_cycle = 0;
   }
}

int timer_i = 0;

void timer_set_hz(int hz) {
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

void timer_handler(registers_t *regs) {

   if(videomode == 0) {
      terminal_writenumat(timer_i%10, 79);
   } else {
      //if(timer_i%1000)
      //   gui_showtimer(timer_i%10);

      if(timer_i%15 == 0) {
         gui_draw();
      }

      if(timer_i%3 == 0) {
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

void closewindow_event(registers_t *regs, void *msg) {
   window_close(regs, (int)msg);
}

void endtask_callback(void *wo, void *regs) {
   // callback for close window dialog
   (void)wo;
   int task = strtoint(getSelectedWindow()->window_objects[0]->text+strlen("Task "));
   events_add(35, &closewindow_event, (void*)get_task_window(task), -1);
   end_task(task, regs);
}

void show_endtask_dialog(int int_no, registers_t *regs) {
   int popup = windowmgr_add();
   char buffer[50];
   sprintf(buffer, "Task %i paused due to exception %i", get_current_task(), int_no);
   window_popup_dialog(getWindow(popup), getWindow(get_current_task_window()), buffer, false, NULL);
   window_disable(getWindow(get_current_task_window()));
   getWindow(popup)->window_objects[1]->click_func = &endtask_callback;
   strcpy(getWindow(popup)->title, "Error");
   strcpy(getWindow(popup)->window_objects[1]->text, "Exit");
   pause_task(get_current_task(), regs);
   toolbar_draw();
   window_draw_outline(getWindow(popup), false);
}

void exception_handler(int int_no, registers_t *regs) {

   cur_regs = regs;

   if(int_no < 32) {

      uint32_t cpl = regs->cs & 0x3;
      bool kernel = cpl == 0;

      if(kernel) {
         debug_printf("Exception %i in kernel mode with eip %i", int_no, regs->eip);

         page_dir_entry_t *dir = gettasks()[get_current_task()].page_dir;

         if(page_getphysical(dir, regs->eip) != (uint32_t)-1) {
            window_writestr(" <", gui_rgb16(255, 100, 100), 0);
            debug_writehex(page_getphysical(dir, regs->eip));
            window_writestr("> ", gui_rgb16(255, 100, 100), 0);

            debug_printf("offset 0x%h\n", regs->eip - KERNEL_START);
            // show hexdump
            int bytes = 32;
            int rowlen = 8;
            
            char *buf = malloc(rowlen);
            buf[rowlen] = '\0';
            uint8_t *mem = (uint8_t*)regs->eip;
            for(int i = 0; i < bytes; i++) {
               if(mem[i] <= 0x0F)
                  debug_printf("0");
               debug_printf("%h ", mem[i]);
               buf[i%rowlen] = mem[i];

               if((i%rowlen) == (rowlen-1) || i==(bytes-1)) {
                  for(int x = 0; x < rowlen; x++) {
                     if(buf[x] != '\n')
                        debug_printf("%c", buf[x]);
                  }
                  debug_printf("\n");
               }
            }
            free((uint32_t)buf, rowlen);

         }
      }

      if(get_current_task() < 0) {
         debug_printf("Exception %i with task %i\n", int_no, get_current_task());
         return;
      }

      // https://wiki.osdev.org/Exceptions
      if(videomode == 1) {

         if(int_no == 14) {
            // page error
            page_dir_entry_t *dir = gettasks()[get_current_task()].page_dir;

            uint32_t addr;
	         asm volatile("mov %%cr2, %0" : "=r" (addr));
            window_writestr("Page fault at ", gui_rgb16(255, 100, 100), 0);
            debug_writehex(addr);
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

               uint32_t offset = page_getphysical(dir, regs->eip) - gettasks()[get_current_task()].prog_start;
               debug_printf(" offset 0x%h, ", offset);
            }

            window_writestr("ebp ", gui_rgb16(255, 100, 100), 0);
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

            window_writestr("\n", 0, 0);

         }

         char buffer[200];
         gui_drawrect(gui_rgb16(180, 0, 0), 60, 0, 8*2, 11);
         gui_writenumat(int_no, gui_rgb16(255, 200, 200), 62, 2);

         sprintf(buffer, "0x%h", regs->eip);
         gui_drawrect(gui_rgb16(180, 0, 0), 120, 0, 8*8, 11);
         gui_writestrat(buffer, gui_rgb16(255, 200, 200), 122, 2);

         if(kernel) {
            // show debug window and panic
            setSelectedWindowIndex(0);
            gui_window_t *window = getSelectedWindow();
            window->minimised = false;
            window->needs_redraw = true;
            window_draw(window);
            while(true) {};
         } else {
            window_writestr("Task ", gui_rgb16(255, 100, 100), 0);
            window_writenum(get_current_task(), 0, 0);
            window_writestr(" paused due to exception ", gui_rgb16(255, 100, 100), 0);
            window_writenum(int_no, 0, 0);
            window_writestr("\n", 0, 0);

            window_resetfuncs(&(gui_get_windows()[get_current_task_window()]));
            show_endtask_dialog(int_no, regs); // + pauses task
         }
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

registers_t *get_regs() {
   return cur_regs;
}