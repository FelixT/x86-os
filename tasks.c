#include "tasks.h"
#include "window.h"
#include "windowmgr.h"
#include "paging.h"

uint32_t USR_CODE_SEG = 8*3;
uint32_t USR_DATA_SEG = 8*4;

task_state_t *tasks;
int current_task = 0;
bool switching = false; // preemptive multitasking

void create_task_entry(int index, uint32_t entry, uint32_t size, bool privileged) {
   //if(tasks[index].enabled) end_task(index, NULL);

   // clear stack

   tasks[index].enabled = false;
   tasks[index].stack_top = (uint32_t)(TOS_PROGRAM - (TASK_STACK_SIZE * index));
   tasks[index].prog_start = entry;
   tasks[index].prog_entry = entry;
   tasks[index].prog_size = size;
   tasks[index].privileged = privileged;
   tasks[index].vmem_start = 0;
   tasks[index].vmem_end = 0;
   tasks[index].in_routine = false;
   tasks[index].page_dir = page_get_kernel_pagedir(); // default to kernel pagedir
   
   tasks[index].registers.esp = tasks[index].stack_top;
   tasks[index].registers.ebp = tasks[index].stack_top;
   tasks[index].registers.eax = USR_DATA_SEG | 3;
   tasks[index].registers.eip = entry;
}

void launch_task(int index, registers_t *regs, bool focus) {
   current_task = index;

   tasks[current_task].registers.ds = USR_DATA_SEG | 3;
   tasks[current_task].registers.cs = USR_CODE_SEG | 3; // user code segment

   tasks[current_task].registers.esp = tasks[current_task].stack_top; // ignored
   tasks[current_task].registers.eflags = regs->eflags;
   tasks[current_task].registers.useresp = tasks[current_task].stack_top; // stack_top
   tasks[current_task].registers.ss = USR_DATA_SEG | 3;

   int tmpwindow = getSelectedWindowIndex();
   tasks[current_task].window = windowmgr_add();
   if(!focus) setSelectedWindowIndex(tmpwindow);

   debug_writestr("Launching task ");
   debug_writeuint(index);
   debug_writestr("\n");

   tasks[current_task].enabled = true;

   swap_pagedir(tasks[current_task].page_dir);
   
   *regs = tasks[current_task].registers;
}

bool task_exists() {
   for(int i = 0; i < TOTAL_TASKS; i++) {
      if(tasks[i].enabled) return true;
   }
   return false;
}

int get_free_task_index() {
   for(int i = 0; i < TOTAL_TASKS; i++)
      if(!tasks[i].enabled) return i;
      
   return -1;
}

void end_task(int index, registers_t *regs) {
   if(index < 0 || index >= TOTAL_TASKS) return;
   if(!tasks[index].enabled) {
      debug_writestr("Task ");
      debug_writeuint(index);
      debug_writestr(" already ended\n");
      return;
   }

   debug_writestr("Ending task ");
   debug_writeuint(index);
   debug_writestr("\n");
   debug_writestr("Current task is ");
   debug_writeuint(get_current_task());
   debug_writestr("\n");

   if(tasks[index].in_routine)
      debug_writestr("Task was in routine\n");

   if(tasks[index].window >= 0)
      window_writestr("Task ended\n", 0, tasks[index].window);

   // todo: kill associated events

   tasks[index].enabled = false;
   tasks[index].privileged = false;

   // free task memory
   if(tasks[index].prog_size != 0)
      free(tasks[index].prog_start, tasks[index].prog_size);
   // TODO: free args

   if(tasks[index].vmem_start != 0) {
      debug_writestr("Unmapping ");
      debug_writehex(tasks[index].vmem_start);
      debug_writestr(" - ");
      debug_writehex(tasks[index].vmem_end);
      debug_writestr("\n");

      for(uint32_t i = tasks[index].vmem_start; i < tasks[index].vmem_end; i++) {
         unmap(tasks[index].page_dir, i);
      }
   }

   tasks[index].window = -1;

   if(index == get_current_task() || !task_exists())
      if(regs != NULL) switch_task(regs);
}

void tasks_alloc() {
   tasks = malloc(sizeof(task_state_t) * TOTAL_TASKS);
}

void tasks_launch_binary(registers_t *regs, char *path) {
   fat_dir_t *entry = fat_parse_path(path);
   if(entry == NULL) {
      gui_writestr("Program not found\n", 0);
      return;
   }
   uint8_t *prog = fat_read_file(entry->firstClusterNo, entry->fileSize);
   uint32_t progAddr = (uint32_t)prog;
   int index = get_free_task_index();
   create_task_entry(index, progAddr, entry->fileSize, false);
   launch_task(index, regs, false);
}

void tasks_launch_elf(registers_t *regs, char *path, int argc, char **args) {
   fat_dir_t *entry = fat_parse_path(path);
   if(entry == NULL) {
      gui_writestr("Not found\n", 0);
      return;
   }
   uint8_t *prog = fat_read_file(entry->firstClusterNo, entry->fileSize);
   elf_run(regs, prog, argc, args);
   free((uint32_t)prog, entry->fileSize);
}

void tasks_init(registers_t *regs) {
   // enable preemptive multitasking

   setSelectedWindowIndex(0);

   for(int i = 0; i < TOTAL_TASKS; i++) {
      tasks[i].enabled = false;
      tasks[i].window = -1;
   }

   // launch idle process
   window_writestr("Launching idle process\n", 0, 0);
   tasks_launch_binary(regs, "/sys/progidle.bin");
   
   gui_get_windows()[tasks[0].window].minimised = true;
   gui_get_windows()[tasks[0].window].draw_func = NULL;
   strcpy(gui_get_windows()[tasks[0].window].title+1, "IDLE");
   //elf_run(regs, prog, 0, 0, NULL);
   //free((uint32_t)prog, entry->fileSize);

   switching = true;

   /*window_writestr("Entry at ", 0, 0);
   window_writenum(regs->eip, 0, 0);
   window_writestr("\n", 0, 0);*/
}

void switch_task(registers_t *regs) {
   if(!switching)
      return;
      // save registers
   tasks[current_task].registers = *regs;

   if(task_exists()) {
      // find next enabled task (round robin)
      do {
         current_task++;
         current_task%=TOTAL_TASKS;
      } while(!tasks[current_task].enabled);

      if(!tasks[current_task].enabled)
         debug_writestr("Task isn't enabled!\n");

      // swap page
      swap_pagedir(tasks[current_task].page_dir);

      // restore registers
      *regs = tasks[current_task].registers;
   } else {
      tasks_init(regs);
   }
}

bool switch_to_task(int index, registers_t *regs) {
   /*window_writestr("Switching from task ", 0, 0);
   window_writenum(current_task, 0, 0);
   window_writestr(" to ", 0, 0);
   window_writenum(index, 0, 0);
   window_writestr("\n", 0, 0);*/

   if(!tasks[index].enabled) {
      window_writestr("Task switch failed: task is unavaliable\n", 0, 0);
      return false;
   }

   // save registers
   tasks[current_task].registers = *regs;

   current_task = index;

   // swap page
   swap_pagedir(tasks[current_task].page_dir);

   // restore registers
   *regs = tasks[current_task].registers;

   return true;
}

task_state_t *gettasks() {
   return &tasks[0];
}

int get_current_task_window() {
   return tasks[current_task].window;
}

int get_current_task() {
   return current_task;
}

int get_task_from_window(int windowIndex) {
   for(int i = 0; i < TOTAL_TASKS; i++) {
      if(tasks[i].window == windowIndex && tasks[i].enabled)
         return i;
   }
   return -1;
}

void task_call_subroutine(registers_t *regs, uint32_t addr, uint32_t *args, int argc) {

   if(tasks[current_task].in_routine || !tasks[current_task].enabled) {
      debug_writestr("Task ");
      debug_writeuint(current_task);
      if(tasks[current_task].in_routine)
         debug_writestr(" is already in a subroutine, returning.\n");
      if(!tasks[current_task].enabled)
         debug_writestr(" not enabled.\n");
      return;
   }

   // save registers

   tasks[current_task].routine_return_regs = *regs;

   tasks[current_task].routine_args = args;
   tasks[current_task].routine_argc = argc;

   // push arguments to stack
   for(int i = 0; i < argc; i++) {
      regs->useresp -= 4;
      ((uint32_t*)regs->useresp)[0] = args[i];
   }

   // push unused return address to stack
   regs->useresp -= 4;
   ((uint32_t*)regs->useresp)[0] = regs->eip;

   // simulate JMP

   // update eip to func addr
   regs->eip = addr;

   tasks[current_task].in_routine = true;
}

void task_subroutine_end(registers_t *regs) {
   // restore registers
   //debug_writestr("Ending subrouting\n");

   *regs = tasks[current_task].routine_return_regs;

   free((uint32_t)tasks[current_task].routine_args, tasks[current_task].routine_argc*sizeof(uint32_t));

   tasks[current_task].in_routine = false;
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