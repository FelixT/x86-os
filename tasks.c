#include "tasks.h"
#include "window.h"
#include "windowmgr.h"
#include "paging.h"
#include "events.h"

task_state_t *tasks;
int current_task = -1;
bool switching = false; // preemptive multitasking

process_t *create_process(uint32_t entry, uint32_t size, bool privileged) {
   process_t *process = malloc(sizeof(process_t));
   process->prog_entry = entry;
   process->prog_size = size;
   process->privileged = privileged;
   process->vmem_start = 0;
   process->vmem_end = 0;
   process->page_dir = page_get_kernel_pagedir(); // default to kernel pagedir
   process->no_allocated = 0;
   process->fd_count = 0;
   strcpy(process->working_dir, "/sys");
   strcpy(process->exe_path, "");
   process->no_threads = 0;
   process->event_queue_size = 0;
   
   return process;
}

void create_task_entry(int index, uint32_t entry, uint32_t size, bool privileged, process_t *process) {
   //if(tasks[index].enabled) end_task(index, NULL);
   // todo: clear stack

   tasks[index].task_id = index;
   tasks[index].enabled = false;
   tasks[index].paused = false;
   tasks[index].stack_top = (uint32_t)(TOS_PROGRAM - (TASK_STACK_SIZE * index));
   //tasks[index].kernel_stack_top = (uint32_t)(TOS_KERNEL - (TASK_STACK_SIZE * index));
   tasks[index].in_routine = false;
   tasks[index].in_syscall = false;
   
   tasks[index].registers.esp = tasks[index].stack_top;
   tasks[index].registers.ebp = tasks[index].stack_top;
   tasks[index].registers.eip = entry;

   if(process == NULL) {
      // launching new process with main thread
      tasks[index].process = create_process(entry, size, privileged);
      tasks[index].process->threads[0] = &tasks[index];
      tasks[index].process->no_threads++;
   } else {
      // launching new thread of existing process
      tasks[index].process = process;
      process->threads[process->no_threads++] = &tasks[index];
   }
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
   tasks[current_task].process->window = windowmgr_add();
   if(focus) { 
      window_draw_outline(getSelectedWindow(), false);
   }

   // setup stdio
   fs_file_t *stdin = fs_open("/dev/stdin");
   fs_file_t *stdout = fs_open("/dev/stdout");
   fs_file_t *stderr = fs_open("/dev/stderr");
   task->process->file_descriptors[0] = stdin;
   task->process->file_descriptors[1] = stdout;
   task->process->file_descriptors[2] = stderr;
   task->process->fd_count = 3;

   if(!focus) { 
      setSelectedWindowIndex(tmpwindow);
   }
   
   debug_printf("Launching task %u\n", index);

   // save regs
   if(old_task >= 0) {
      tasks[old_task].registers = *regs;
   }

   tasks[current_task].enabled = true;

   swap_pagedir(get_current_task_pagedir());
   
   *regs = tasks[current_task].registers;

   //extern tss_t tss_start;
   //tss_start.esp0 = tasks[current_task].kernel_stack_top;
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

   task_reset_windows(index);

   tasks[index].paused = true;
   if(index == get_current_task() || !task_exists())
      if(regs != NULL) switch_task(regs);
}

void task_reset_windows(int task) {
   // remove funcs from all associated windows
   int taskw = tasks[task].process->window;
   if(taskw >= 0 && taskw < getWindowCount() && !getWindow(taskw)->closed) {
      gui_window_t *window = getWindow(taskw);
      window_resetfuncs(window);
      for(int i = 0; i < window->child_count; i++) {
         gui_window_t *child = (gui_window_t*)window->children[i];
         if(!child || child->closed) continue;
         window_resetfuncs(child);
      }
   }
}

void end_task(int index, registers_t *regs) {
   if(index < 0 || index >= TOTAL_TASKS) return;

   task_state_t *task = &tasks[index];
   if(!task->enabled) {
      debug_printf("Task %u already ended\n", index);
      return;
   }

   debug_printf("Ending task %i - Current task is %i\n", index, get_current_task());

   if(task->in_routine)
      debug_printf("Task was in %sroutine %s\n", (task->routine_return_window>=0 ? "queued " : ""), task->routine_name);

   if(task->in_syscall)
      debug_printf("Task was in syscall %i\n", task->syscall_no);

   if(regs != NULL && (index == get_current_task() || !task_exists()))
      switch_task(regs); // swap page dir before freeing

   if(task->process->threads[0] == task) {
      // main thread, terminate entire process
      debug_printf("Ending task process\n");

      // free task memory
      if(task->process->prog_size != 0)
         free(task->process->prog_start, task->process->prog_size);

      // free args
      for(int i = 0; i < task->routine_argc; i++) {
         free(task->routine_args[i], PAGE_SIZE);
      }
      
      // free events
      for(int i = 0; i < task->process->event_queue_size; i++) {
         task_event_t *event = task->process->event_queue[i];
         if(event)
            free((uint32_t)event, sizeof(task_event_t));
      }

      // free fds
      for(int i = 0; i < task->process->fd_count; i++) {
         fs_file_t *fd = task->process->file_descriptors[i];
         fs_close(fd);
      }

      // kill other tasks
      debug_printf("Ending %i other threads of process\n", task->process->no_threads - 1);
      for(int i = 1; i < task->process->no_threads; i++) {
         task_state_t *thread = task->process->threads[i];
         end_task(thread->task_id, regs);
         thread->process = NULL;
      }

      if(task->process->window >= 0)
         task_write_to_window(index, "<Task ended>\n", true);

      task_reset_windows(index);

      // free heap
      if(task->process->heap_end > task->process->heap_start) {
         for(uint32_t addr = task->process->heap_start; addr < task->process->heap_end; addr+=PAGE_SIZE) {
            free(page_getphysical(task->process->page_dir, addr), PAGE_SIZE);
         }
      }
      free_page_dir(task->process->page_dir);

      // free process
      free((uint32_t)task->process, sizeof(process_t));
      task->process = NULL;
   }

   task->enabled = false;
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
   create_task_entry(index, progAddr, entry->fileSize, false, NULL);
   launch_task(index, regs, false);
   gui_redrawall();
}

bool tasks_launch_elf(registers_t *regs, char *path, int argc, char **args, bool focus) {
   fat_dir_t *entry = fat_parse_path(path, true);
   if(entry == NULL) {
      gui_writestr("Not found\n", 0);
      return false;
   }
   uint8_t *prog = fat_read_file(entry->firstClusterNo, entry->fileSize);
   elf_run(regs, prog, entry->fileSize, argc, args, focus);
   strcpy(get_current_task_state()->process->exe_path, path);
   free((uint32_t)prog, entry->fileSize);
   free((uint32_t)entry, sizeof(fat_dir_t));
   return true;
}

void tasks_init(registers_t *regs) {
   // enable preemptive multitasking

   setSelectedWindowIndex(0);

   for(int i = 0; i < TOTAL_TASKS; i++) {
      tasks[i].enabled = false;
      //tasks[i].process->window = -1; hmm
   }

   // launch idle process
   tasks_launch_binary(regs, "/sys/progidle.bin");
   
   gui_get_windows()[tasks[0].process->window].minimised = true;
   gui_get_windows()[tasks[0].process->window].draw_func = NULL;
   strcpy(gui_get_windows()[tasks[0].process->window].title, "Idle Process");
   //elf_run(regs, prog, 0, 0, NULL);
   //free((uint32_t)prog, entry->fileSize);

   switching = true;
}

void switch_task(registers_t *regs) {
   if(!switching)
      return;

   int old_task = current_task;

   // find next enabled task (round robin)
   do {
      current_task++;
      current_task%=TOTAL_TASKS;

      if(current_task == old_task && (!tasks[current_task].enabled || tasks[current_task].paused)) {
         // no tasks, launch idle process
         debug_printf("No tasks found\n");
         // save registers
         tasks[old_task].registers = *regs;
         tasks_init(regs);
         break;
      }
   } while(!tasks[current_task].enabled || tasks[current_task].paused);
   
   task_state_t *task = get_current_task_state();
   if(task->process->page_dir != page_get_current())
      swap_pagedir(task->process->page_dir);

   if(old_task != current_task) {
      // save registers
      tasks[old_task].registers = *regs;

      // restore registers
      *regs = tasks[current_task].registers;

      //extern tss_t tss_start;
      //tss_start.esp0 = tasks[current_task].kernel_stack_top;
   }
}

bool switch_to_task(int index, registers_t *regs) {
   //debug_printf("Switching from task %i to %i\n", current_task, index);
   if(index < 0 || index > TOTAL_TASKS) {
      debug_printf("Invalid task %i\n", index);
      return false;
   }

   if(!tasks[index].enabled) {
      debug_printf("Task switch failed: task %i is unavailable\n", index);
      return false;
   }
   if(tasks[index].paused) {
      debug_printf("Task switch failed: task %i is paused\n", index);
      return false;
   }

   int old_task = current_task;

   current_task = index;

   // swap page
   if(page_get_current() != get_current_task_pagedir())
      swap_pagedir(get_current_task_pagedir());

   if(current_task != old_task) {
      // save registers
      tasks[old_task].registers = *regs;

      // restore registers
      *regs = tasks[current_task].registers;

      //extern tss_t tss_start;
      //tss_start.esp0 = tasks[current_task].kernel_stack_top;
   }

   return true;
}

task_state_t *gettasks() {
   return &tasks[0];
}

int get_current_task_window() {
   return tasks[current_task].process->window;
}

int get_task_window(int task) {
   return tasks[task].process->window;
}

int get_current_task() {
   return current_task;
}

task_state_t *get_current_task_state() {
   return &tasks[current_task];
}

page_dir_entry_t *get_current_task_pagedir() {
   return get_current_task_state()->process->page_dir;
}

int get_task_from_window(int windowIndex) {
   for(int i = 0; i < TOTAL_TASKS; i++) {
      task_state_t *task = &tasks[i];
      if(!task->enabled) continue;
      if(task->process->window == windowIndex) {
         return i;
      } else {
         gui_window_t *searchWindow = getWindow(windowIndex);
         gui_window_t *mainWindow = getWindow(task->process->window);
         if(!mainWindow || mainWindow->closed) continue;
         // check children of window
         for(int x = 0; x < mainWindow->child_count; x++) {
            gui_window_t *child = mainWindow->children[x];
            if(child != NULL && child->state != NULL) // dialog
               continue;
            if(child == searchWindow)
               return i;
         }
      }
   }
   return -1;
}

void task_execute_subroutine(registers_t *regs, char *name, uint32_t addr, uint32_t *args, int argc) {
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

void task_execute_queued_subroutine(void *regs, void *msg) {
   // check events queue

   int taskid = (int)msg;
   task_state_t *task = &tasks[taskid];
   if(task->in_routine) {
      // do nothing - wait until task_subroutine_end to call this function
   } else {

      if(task->process->event_queue_size > 0) {
         if(!switch_to_task(taskid, regs)) return;

         task_event_t *first_event = task->process->event_queue[0];

         task_execute_subroutine(regs, first_event->name, first_event->addr, first_event->args, first_event->argc);

         task->process->event_queue_size--;

         free((uint32_t)task->process->event_queue[0], sizeof(task_event_t));

         // todo: circular queue is faster than memmove
         memmove(&task->process->event_queue[0], &task->process->event_queue[1], task->process->event_queue_size * sizeof(task_event_t*));

         task->routine_return_window = getSelectedWindowIndex();

         /*if(get_current_task_window() != getSelectedWindowIndex())
            setSelectedWindowIndex(get_current_task_window());*/
      }
   }
}

void task_queue_subroutine(task_state_t *task, char *name, uint32_t addr, uint32_t *args, int argc) {
   if(!task->enabled) {
      debug_printf("Couldn't queue routine for task %i - task is disabled\n", task->task_id);
      free((uint32_t)args, sizeof(uint32_t*)*argc);
      return;
   }
   if(task->process->event_queue_size == EVENT_QUEUE_SIZE) {
      debug_printf("Task %i hit maximum event queue size with event %s\n", task->task_id, name);
      free((uint32_t)args, sizeof(uint32_t*)*argc);
      return;
   }
   // add to event queue
   task_event_t *event = (task_event_t*)malloc(sizeof(task_event_t));
   strcpy(event->name, name);
   event->addr = addr;
   event->args = args;
   event->argc = argc;
   event->task = current_task;
   task->process->event_queue[task->process->event_queue_size++] = event;
}

void task_call_subroutine(registers_t *regs, task_state_t *task, char *name, uint32_t addr, uint32_t *args, int argc) {

   // call subroutine immediately, switching to task
   
   if(!task->enabled) {
      debug_printf("Task %i is ended, exiting subroutine", task->task_id);
      free((uint32_t)args, sizeof(uint32_t*)*argc);
      return;
   }

   if(task->paused || task->in_routine) {
      task_queue_subroutine(task, name, addr, args, argc);
      return;
   }
   
   if(!switch_to_task(task->task_id, regs)) {
      free((uint32_t)args, sizeof(uint32_t*)*argc);
      return;
   }

   // if event queue is empty, launch into routine immediately
   // otherwise just wait for queued event
   if(task->process->event_queue_size > 0) {
      debug_printf("Not in routine but queue has content\n");
      task_queue_subroutine(task, name, addr, args, argc);
      task_execute_queued_subroutine(regs, (void*)current_task);
   } else {
      task_execute_subroutine(regs, name, addr, args, argc);
   }
}

void task_subroutine_end(registers_t *regs) {
   // restore registers
   //debug_writestr("Ending subrouting\n");
   if(!tasks[current_task].in_routine)
      return;

   *regs = tasks[current_task].routine_return_regs;

   free((uint32_t)tasks[current_task].routine_args, tasks[current_task].routine_argc*sizeof(uint32_t*));

   tasks[current_task].in_routine = false;
   // check for any other queued events and run if there are
   task_execute_queued_subroutine(regs, (void*)current_task);

   if(tasks[current_task].routine_return_window >= 0)
      setSelectedWindowIndex(tasks[current_task].routine_return_window);

   if(tasks[current_task].paused) {
      switch_task(regs); // yield
   }
}

void task_write_to_window(int task, char *out, bool children) {
   task_state_t *t = &tasks[task];
   // write to stdio
   int w = t->process->file_descriptors[1]->window_index;
   int curw = get_current_task_window();
   if(w == curw || (w >= 0 && w < getWindowCount() && !getWindow(w)->closed)) {
      gui_window_t *window = getWindow(w);
      window_writestr(out, window->txtcolour, w);
      if(children) {
         for(int i = 0; i < window->child_count; i++) {
            window_writestr(out, window->txtcolour, get_window_index_from_pointer(window->children[i]));
         }
      }
   } else {
      debug_printf("Tried to write '%s' to task %i window %i\n", out, task, w);
   }
}

void tss_init() {
   // setup tss entry in gdt
   extern gdt_entry_t gdt_tss;
   extern tss_t tss_start;
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

int current_servicing_task = -1;

bool copy_to_task(int task, void *dest, void *src, size_t size) {
   task_state_t *task_state = &gettasks()[task];
   process_t *process = task_state->process;
   if((uint32_t)dest < process->heap_start || ((uint32_t)dest + size) > process->heap_end) {
      return false;
   }
   current_servicing_task = task;
   page_dir_entry_t *old_dir = page_get_current();
   page_dir_entry_t *task_dir = task_state->process->page_dir;
   bool swapped = task_dir != old_dir;
   if(swapped) swap_pagedir(task_dir);
   memcpy(dest, src, size);
   if(swapped) swap_pagedir(old_dir);
   current_servicing_task = -1;
   return true;
}