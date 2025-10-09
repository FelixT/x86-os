#include "tasks.h"
#include "window.h"
#include "windowmgr.h"
#include "paging.h"
#include "events.h"

uint32_t USR_CODE_SEG = 8*3;
uint32_t USR_DATA_SEG = 8*4;

task_state_t *tasks;
int current_task = 0;
bool switching = false; // preemptive multitasking

void create_task_entry(int index, uint32_t entry, uint32_t size, bool privileged) {
   //if(tasks[index].enabled) end_task(index, NULL);

   // clear stack

   tasks[index].enabled = false;
   tasks[index].paused = false;
   tasks[index].stack_top = (uint32_t)(TOS_PROGRAM - (TASK_STACK_SIZE * index));
   tasks[index].prog_start = entry;
   tasks[index].prog_entry = entry;
   tasks[index].prog_size = size;
   tasks[index].privileged = privileged;
   tasks[index].vmem_start = 0;
   tasks[index].vmem_end = 0;
   tasks[index].in_routine = false;
   tasks[index].in_syscall = false;
   tasks[index].page_dir = page_get_kernel_pagedir(); // default to kernel pagedir
   tasks[index].no_allocated = 0;

   tasks[index].fd_count = 0;
   
   tasks[index].registers.esp = tasks[index].stack_top;
   tasks[index].registers.ebp = tasks[index].stack_top;
   tasks[index].registers.eip = entry;

   strcpy(tasks[index].working_dir, "/sys");
}

void launch_task(int index, registers_t *regs, bool focus) {
   int old_task = current_task;

   current_task = index;

   task_state_t *task = &tasks[current_task];

   task->registers.ds = USR_DATA_SEG | 3;
   task->registers.cs = USR_CODE_SEG | 3; // user code segment
   task->registers.eflags = regs->eflags;
   task->registers.useresp = tasks[current_task].stack_top; // stack_top
   task->registers.ss = USR_DATA_SEG | 3;

   int tmpwindow = getSelectedWindowIndex();
   tasks[current_task].window = windowmgr_add();
   if(!focus) { 
      setSelectedWindowIndex(tmpwindow);
   } else {
      window_draw_outline(getSelectedWindow(), false);
   }

   // setup stdio
   fs_file_t *stdin = fs_open("/dev/stdin");
   fs_file_t *stdout = fs_open("/dev/stdout");
   fs_file_t *stderr = fs_open("/dev/stderr");
   task->file_descriptors[0] = stdin;
   task->file_descriptors[1] = stdout;
   task->file_descriptors[2] = stderr;
   task->fd_count = 3;
   
   debug_printf("Launching task %u\n", index);

   // save regs
   if(old_task >= 0) {
      tasks[old_task].registers = *regs;
   }

   tasks[current_task].enabled = true;

   swap_pagedir(tasks[current_task].page_dir);
   
   *regs = tasks[current_task].registers;
}

bool task_exists() {
   for(int i = 0; i < TOTAL_TASKS; i++) {
      if(tasks[i].enabled && !tasks[i].paused) return true;
   }
   return false;
}

int get_free_task_index() {
   for(int i = 0; i < TOTAL_TASKS; i++)
      if(!tasks[i].enabled) return i;
      
   return -1;
}

void pause_task(int index, registers_t *regs) {
   if(index < 0 || index >= TOTAL_TASKS) return;
   if(!tasks[index].enabled) {
      debug_printf("Task %u already ended\n", index);
      return;
   }

   tasks[index].paused = true;
   if(index == get_current_task() || !task_exists())
      if(regs != NULL) switch_task(regs);
}

void end_task(int index, registers_t *regs) {
   if(index < 0 || index >= TOTAL_TASKS) return;
   if(!tasks[index].enabled) {
      debug_printf("Task %u already ended\n", index);
      return;
   }

   debug_printf("Ending task %i - Current task is %i\n", index, get_current_task());

   if(tasks[index].in_routine)
      debug_printf("Task was in %sroutine %s\n", (tasks[index].routine_return_window>=0 ? "queued " : " "), tasks[index].routine_name);

   if(tasks[index].in_syscall)
      debug_printf("Task was in syscall %i\n", tasks[index].syscall_no);

   if(tasks[index].window >= 0)
      task_write_to_window(index, "<Task ended>\n");

   // todo: kill associated events

   tasks[index].enabled = false;
   tasks[index].privileged = false;

   // free task memory
   if(tasks[index].prog_size != 0)
      free(tasks[index].prog_start, tasks[index].prog_size);
   // TODO: free args
   // TODO: free page dir at some point
   // TODO: free fds

   if(tasks[index].vmem_start != 0) {
      debug_printf("Unmapping 0x%h - 0x%h\n", tasks[index].vmem_start, tasks[index].vmem_end);

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
   int index = get_free_task_index();
   if(index == -1) {
      debug_printf("No free tasks\n");
      return;
   }
   fat_dir_t *entry = fat_parse_path(path, true);
   if(entry == NULL) {
      gui_writestr("Program not found\n", 0);
      return;
   }
   uint8_t *prog = fat_read_file(entry->firstClusterNo, entry->fileSize);
   uint32_t progAddr = (uint32_t)prog;
   create_task_entry(index, progAddr, entry->fileSize, false);
   launch_task(index, regs, false);
   gui_redrawall();
}

void tasks_launch_elf(registers_t *regs, char *path, int argc, char **args) {
   fat_dir_t *entry = fat_parse_path(path, true);
   if(entry == NULL) {
      gui_writestr("Not found\n", 0);
      return;
   }
   uint8_t *prog = fat_read_file(entry->firstClusterNo, entry->fileSize);
   elf_run(regs, prog, entry->fileSize, argc, args);
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
   strcpy(gui_get_windows()[tasks[0].window].title, "Idle Process");
   //elf_run(regs, prog, 0, 0, NULL);
   //free((uint32_t)prog, entry->fileSize);

   switching = true;
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
      } while(!tasks[current_task].enabled || tasks[current_task].paused);

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
   //debug_printf("Switching from task %i to %i\n", current_task, index);

   if(!tasks[index].enabled) {
      debug_printf("Task switch failed: task %i is unavailable\n", index);
      return false;
   }
   if(tasks[index].paused) {
      debug_printf("Task switch failed: task %i is paused\n", index);
      return false;
   }

   // save registers
   tasks[current_task].registers = *regs;

   current_task = index;

   // swap page
   if(page_get_current() != tasks[current_task].page_dir)
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

int get_task_window(int task) {
   return tasks[task].window;
}

int get_current_task() {
   return current_task;
}

task_state_t *get_current_task_state() {
   return &tasks[current_task];
}

int get_task_from_window(int windowIndex) {
   for(int i = 0; i < TOTAL_TASKS; i++) {
      if(tasks[i].window == windowIndex && tasks[i].enabled)
         return i;
   }
   return -1;
}

typedef struct {
   char *name;
   uint32_t addr; // subroutine addr
   uint32_t *args;
   int argc;
   int task;
} task_event_t;

void task_execute_queued_subroutine(void *regs, void *msg) {
   // callback from queued event
   task_event_t *event = (task_event_t*)msg;
   if(tasks[current_task].in_routine) {
      // queue back up again
      events_add(15, &task_execute_queued_subroutine, (void*)event, -1);
   } else {

      if(!switch_to_task(event->task, regs)) return;

      task_call_subroutine(regs, event->name, event->addr, event->args, event->argc);
      tasks[event->task].routine_return_window = getSelectedWindowIndex();

      /*debug_printf("Launching queued routine %s for task %i window %i\n", event->name, get_current_task(), get_current_task_window());
      debug_printf("Arguments are ");
      for(int i = 0; i < event->argc; i++) {
         debug_printf("%i <0x%h> ", i, event->args[i]);
      }
      debug_printf("\n");*/

      if(get_current_task_window() != getSelectedWindowIndex()) {
         setSelectedWindowIndex(get_current_task_window());
      }
   }
}

void task_call_subroutine(registers_t *regs, char *name, uint32_t addr, uint32_t *args, int argc) {

   if(tasks[current_task].in_routine) {
      if(strequ(name, tasks[current_task].routine_name))
         return; // don't queue up same event until current handler is done
      if(strstartswith(tasks[current_task].routine_name, "wo") && strequ(tasks[current_task].routine_name+2, name))
         return; // wo event overrides main event
      task_event_t *event = (task_event_t*)malloc(sizeof(task_event_t));
      event->name = name;
      event->addr = addr;
      event->args = args;
      event->argc = argc;
      event->task = current_task;
      events_add(10, &task_execute_queued_subroutine, (void*)event, -1);
      return;
   } else if(!tasks[current_task].enabled) {
      debug_printf("Task %i is ended, exiting subroutine");
      return;
   }

   strcpy(tasks[current_task].routine_name, name);

   // save registers

   tasks[current_task].routine_return_regs = *regs;
   tasks[current_task].routine_return_window = -1;

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
   if(!tasks[current_task].in_routine)
      return;

   *regs = tasks[current_task].routine_return_regs;

   free((uint32_t)tasks[current_task].routine_args, tasks[current_task].routine_argc*sizeof(uint32_t*));

   tasks[current_task].in_routine = false;

   if(tasks[current_task].routine_return_window >= 0)
      setSelectedWindowIndex(tasks[current_task].routine_return_window);

   if(tasks[current_task].paused) {
      switch_task(regs); // yield
   }
}

void task_write_to_window(int task, char *out) {
   task_state_t *t = &gettasks()[task];
   // write to stdio
   int w = t->file_descriptors[1]->window_index;
   int curw = get_current_task_window();
   if(w == curw || (w >= 0 && w < getWindowCount() && getWindow(w) && !getWindow(w)->closed)) {
      window_writestr(out, getWindow(w)->txtcolour, w);
   } else {
      debug_printf("Tried to write '%s' to task %i window %i\n", out, task, w);
   }
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