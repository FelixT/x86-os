#include "tasks.h"
#include "window.h"
#include "windowmgr.h"
#include "paging.h"
#include "events.h"
#include "fs.h"
#include "shared.h"
#include "msg.h"

task_state_t *tasks;
int current_task = -1;
bool switching = false; // preemptive multitasking

uint32_t process_uid_counter = 0;
uint32_t task_uid_counter = 0;

process_t *create_process(uint32_t entry, uint32_t size, bool privileged) {
   process_t *process = malloc(sizeof(process_t));
   process->uid = process_uid_counter++;
   process->prog_entry = entry;
   process->prog_start = entry;
   process->prog_size = size;
   process->privileged = privileged;
   process->vmem_start = 0;
   process->vmem_end = 0;
   process->page_dir = page_get_kernel_pagedir(); // default to kernel pagedir
   process->no_allocated = 0;
   process->fd_count = 0;
   for(int i = 0; i < PROCESS_MAX_FDS; i++)
      process->file_descriptors[i] = NULL;
   process->mmio_end = V_MMIO_START;
   process->device_count = 0;
   process->dma_count = 0;
   process->port_count = 0;
   strcpy(process->working_dir, "/sys");
   strcpy(process->exe_path, "");
   process->no_threads = 0;
   for(int i = 0; i < MAX_TASK_THREADS; i++)
      process->threads[i] = NULL;
   process->event_queue_size = 0;
   process->heap_start = 0;
   process->heap_end = 0;
   process->launch_argc = 0;
   process->launch_args = NULL;
   
   return process;
}

// returns thread no
int create_task_entry(int index, uint32_t entry, uint32_t size, bool privileged, process_t *process) {
   // lazy allocated stacks
   if(!tasks[index].stack_base) {
      void *stack = malloc(TASK_STACK_SIZE - 0x1000); // don't need to allocate physical memory for page guard
      if(!stack) {
         debug_printf("couldn't allocate stack for task %i\n", index);
         return -1;
      }
      tasks[index].stack_base = (uint32_t)stack;
   }
   if(!tasks[index].kernel_stack_top) {
      void *kernel_stack = malloc(KSTACK_SIZE); // do need to allocate physical memory for page guard (currently)
      if(!kernel_stack) {
         debug_printf("couldn't allocate kernel stack for task %i\n", index);
         return -1;
      }
      tasks[index].kernel_stack_top = (uint32_t)kernel_stack + KSTACK_SIZE;
   }
   memset((void*)tasks[index].stack_base, 0, TASK_STACK_SIZE - 0x1000); // clear stack

   int thread_no = -1;
   if(process == NULL) {
      // launching new process with main thread
      tasks[index].process = create_process(entry, size, privileged);
      thread_no = 0;
      tasks[index].process->threads[thread_no] = &tasks[index];
   } else {
      // launching new thread of existing process
      for(int i = 0; i < MAX_TASK_THREADS; i++) {
         if(process->threads[i] != NULL) continue;
         thread_no = i;
         break;
      }
      if(thread_no == -1) {
         debug_printf("process %u hit maximum number of threads\n", process->uid);
         return -1;
      }
      tasks[index].process = process;
      process->threads[thread_no] = &tasks[index];
   }

   tasks[index].task_id = index;
   tasks[index].task_uid = task_uid_counter++;
   tasks[index].enabled = false;
   tasks[index].paused = false;
   tasks[index].pause_reason = PAUSE_NONE;
   tasks[index].wake_pending = false;
   tasks[index].crashed = false;
   tasks[index].in_routine = false;
   tasks[index].in_syscall = false;
   tasks[index].msg_func = NULL;
   tasks[index].msg_channel_count = 0;
   tasks[index].msg_notify_pending = false;
   
   tasks[index].registers.eip = entry;
   tasks[index].process->no_threads++;
   return thread_no;
}

// map task's user stack into its process page dir and set up the kernel stack guard
// (kernel stack is in heap so already mapped to kernel)
bool task_map_stack(task_state_t *task, int thread_no) {
   page_dir_entry_t *dir = task->process->page_dir;
   int slot = (dir == page_get_kernel_pagedir()) ? task->task_id : thread_no;
   task->v_stack_start = V_STACKS_START + slot*TASK_STACK_SIZE;

   // setup program stack, already mallocd by create_task_entry, vmap to V_STACK_START for main thread
   for(uint32_t i = 1; i < TASK_STACK_SIZE/0x1000; i++) {
      if(!map(dir, task->stack_base + (i-1)*0x1000, task->v_stack_start + i*0x1000, 1, 1, 0)) {
         debug_printf("task_map_stack: couldn't map stack for task %i\n", task->task_id);
         for(uint32_t j = 1; j < i; j++)
            unmap(dir, task->v_stack_start + j*0x1000);
         return false;
      }
   }

   // kernel stack guard - unmap from heap
   unmap(dir, task->kernel_stack_top - KSTACK_SIZE);

   uint32_t v_stack_top = task->v_stack_start + TASK_STACK_SIZE;
   task->registers.ebp = v_stack_top;
   task->registers.useresp = v_stack_top;
   return true;
}

static void task_unmap_stack(task_state_t *task) {
   page_dir_entry_t *dir = task->process->page_dir;
   for(uint32_t i = 1; i < TASK_STACK_SIZE/0x1000; i++)
      unmap(dir, task->v_stack_start + i*0x1000);

   uint32_t guard = task->kernel_stack_top - KSTACK_SIZE;
   if(dir == page_get_kernel_pagedir())
      map(dir, guard, guard, 1, 1, 0);
   else
      map(dir, guard, guard, 0, 0, 0);
}

bool setup_task_init(int index, registers_t *regs, bool focus, bool open_fds) {
   task_state_t *task = &tasks[index];

   // note: tasks created here are always main thread
   if(!task_map_stack(task, 0))
      return false;

   task->registers.ds = USR_DATA_SEG | 3;
   task->registers.cs = USR_CODE_SEG | 3;
   task->registers.eflags = regs->eflags;
   task->registers.ss = USR_DATA_SEG | 3;

   int tmpwindow = getSelectedWindowIndex();
   task->process->window = windowmgr_add();
   if(focus) {
      window_draw_outline(getSelectedWindow(), false);
   }

   if(open_fds) {
      task->process->file_descriptors[0] = fs_open("/dev/stdin", FS_FLAG_READONLY);
      task->process->file_descriptors[1] = fs_open("/dev/stdout", FS_FLAG_WRITEONLY);
      task->process->file_descriptors[2] = fs_open("/dev/stderr", FS_FLAG_WRITEONLY);
      task->process->fd_count = 3;
   }

   if(!focus)
      setSelectedWindowIndex(tmpwindow);
   return true;
}

bool launch_task(int index, registers_t *regs, bool focus) {
   int old_task = current_task;
   current_task = index;

   if(!setup_task_init(index, regs, focus, true)) {
      current_task = old_task;
      return false;
   }

   if(old_task >= 0 && tasks[old_task].enabled) {
      tasks[old_task].registers = *regs;
   }

   tasks[current_task].enabled = true;
   swap_pagedir(get_current_task_pagedir());
   *regs = tasks[current_task].registers;

   extern tss_t tss_start;
   tss_start.esp0 = tasks[current_task].kernel_stack_top;
   return true;
}

// undo create_task_entry (launch/init failed)
void task_discard_entry(int index) {
   process_t *process = tasks[index].process;
   if(!process) return;

   if(process->prog_size != 0)
      free(process->prog_start, process->prog_size);
   if(process->page_dir != page_get_kernel_pagedir())
      free_page_dir(process->page_dir);
   free((uint32_t)process, sizeof(process_t));
   tasks[index].process = NULL;
}

bool task_exists() {
   for(int i = 0; i < TOTAL_TASKS; i++) {
      if(tasks[i].enabled && !tasks[i].paused) return true;
   }
   return false;
}

int get_free_task_index() {
   for(int i = 0; i < TOTAL_TASKS; i++)
      if(!tasks[i].enabled && i != current_task) return i;
      
   return -1;
}

void pause_task(int index, registers_t *regs) {
   // program has crashed
   if(index < 0 || index >= TOTAL_TASKS) return;
   if(!tasks[index].enabled) {
      debug_printf("Task %u already ended\n", index);
      return;
   }

   task_reset_windows(index);

   task_pause(&tasks[index], PAUSE_CRASH);
   tasks[index].crashed = true;
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

// heap page returned back to kernel only access (set in new_page)
static void unshare_heap_page(page_dir_entry_t *dir, uint32_t addr) {
   addr = page_align_down(addr);
   map(dir, addr, addr, 0, 0, 0);
}

// hacky but less hacky than having task clean this up
static void cleanup_checkcmd_args(process_t *process, uint32_t *args) {
   char *buffer = (char*)args[0];
   if(process) {
      unshare_heap_page(process->page_dir, (uint32_t)buffer);
      unshare_heap_page(process->page_dir, (uint32_t)args);
   }
   free((uint32_t)buffer, TEXT_BUFFER_LENGTH);
}

static void free_process_events(process_t *process) {
   // free all events in queue
   for(int i = 0; i < process->event_queue_size; i++) {
      task_event_t *event = process->event_queue[i];
      if(!event) continue;
      if(strequ(event->name, "checkcmd"))
         cleanup_checkcmd_args(process, event->args);
      if(event->args)
         free((uint32_t)event->args, event->argc * sizeof(uint32_t*));
      free((uint32_t)event, sizeof(task_event_t));
   }
   process->event_queue_size = 0;
}

static void free_task_events(task_state_t *task) {
   // remove events for ended thread of still running process
   int e;
   while((e = task_find_queued_subroutine(task)) >= 0) {
      task_event_t *event = task->process->event_queue[e];
      task_remove_queued_subroutine(task, e);
      msg_retry_notifications(task);
      if(strequ(event->name, "checkcmd"))
         cleanup_checkcmd_args(task->process, event->args);
      if(event->args)
         free((uint32_t)event->args, event->argc * sizeof(uint32_t*));
      free((uint32_t)event, sizeof(task_event_t));
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

   if(task->in_routine) {
      debug_printf("Task was in %sroutine %s\n", (task->routine_return_window>=0 ? "queued " : ""), task->routine_name);
      if(strequ(task->routine_name, "checkcmd"))
         cleanup_checkcmd_args(task->process, task->routine_args);
   }

   if(task->in_syscall)
      debug_printf("Task was in syscall %i\n", task->syscall_no);

   task->enabled = false;
   if(regs != NULL && (index == get_current_task() || !task_exists()))
      switch_task(regs); // swap page dir before freeing

   // free channels
   msg_cleanup_task(task);

   if(task->process->threads[0] == task) {
      // main thread, terminate entire process
      debug_printf("Ending task process\n");

      // free task memory
      if(task->process->prog_size != 0)
         free(task->process->prog_start, task->process->prog_size);

      // free process events
      free_process_events(task->process);

      // free fds
      for(int i = 0; i < task->process->fd_count; i++) {
         fs_file_t *fd = task->process->file_descriptors[i];
         if(fd) fs_close(fd);
      }

      // kill other threads
      debug_printf("Ending %i other threads of process\n", task->process->no_threads - 1);
      for(int i = 1; i < MAX_TASK_THREADS; i++) {
         task_state_t *thread = task->process->threads[i];
         if(!thread) continue;
         end_task(thread->task_id, NULL);
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

      // close shared memory
      shared_cleanup(task->process);

      // silence mapped devices + reclaim DMA buffers
      dma_cleanup(task->process);

      // free ports
      msg_cleanup_process(task->process);

      // free page dir
      if(task->process->page_dir != page_get_kernel_pagedir())
         free_page_dir(task->process->page_dir);
      else // binary task - page dir persists, so undo stack mapping
         task_unmap_stack(task);

      // free launch args
      free_launch_args(task->process->launch_args, task->process->launch_argc);

      // free process
      free((uint32_t)task->process, sizeof(process_t));
      task->process = NULL;
   } else {
      free_task_events(task);

      task_unmap_stack(task);

      // reclaim slot from process threads
      for(int i = 0; i < MAX_TASK_THREADS; i++) {
         if(task->process->threads[i] != task) continue;
         task->process->threads[i] = NULL;
         task->process->no_threads--;
         break;
      }
   }

   if(task->routine_args) {
      free((uint32_t)task->routine_args, task->routine_argc * sizeof(uint32_t*));
      task->routine_args = NULL;
      task->routine_argc = 0;
   }
   task->in_routine = false;
}

void tasks_alloc() {
   tasks = malloc(sizeof(task_state_t) * TOTAL_TASKS);
   for(int i = 0; i < TOTAL_TASKS; i++) {
      tasks[i].enabled = false;
      tasks[i].kernel_stack_top = 0; // stacks are lazy allocated when index is first claimed in create_task_entry and persist end_task
      tasks[i].stack_base = 0;
   }
}

bool tasks_launch_binary(registers_t *regs, char *path) {
   int index = get_free_task_index();
   if(index == -1) {
      debug_printf("No free tasks\n");
      return false;
   }
   fat_dir_t *entry = fat_parse_path(path, true);
   if(entry == NULL) {
      gui_writestr("Program not found\n", 0);
      return false;
   }
   uint8_t *prog = fat_read_file(entry->firstClusterNo, entry->fileSize);
   uint32_t progAddr = (uint32_t)prog;
   if(create_task_entry(index, progAddr, entry->fileSize, false, NULL) == -1) {
      debug_printf("tasks_launch_binary: create task entry failed\n");
      free((uint32_t)prog, entry->fileSize);
      free((uint32_t)entry, sizeof(fat_dir_t));
      return false;
   }
   if(!launch_task(index, regs, false)) {
      debug_printf("tasks_launch_binary: launch failed\n");
      task_discard_entry(index); // frees prog
      free((uint32_t)entry, sizeof(fat_dir_t));
      return false;
   }
   free((uint32_t)entry, sizeof(fat_dir_t));
   gui_redrawall();
   return true;
}

bool tasks_launch_elf(registers_t *regs, char *path, int argc, char **args, bool focus) {
   fat_dir_t *entry = fat_parse_path(path, true);
   if(entry == NULL) {
      gui_writestr("Not found\n", 0);
      return false;
   }
   uint8_t *prog = fat_read_file(entry->firstClusterNo, entry->fileSize);
   bool launched = elf_run(regs, prog, entry->fileSize, argc, args, focus);
   if(launched)
      strcpy(get_current_task_state()->process->exe_path, path);
   free((uint32_t)prog, entry->fileSize);
   free((uint32_t)entry, sizeof(fat_dir_t));
   return launched;
}

// doesn't immediately switch, caller is responsible for enabling the task
int tasks_setup_elf(registers_t *regs, char *path, int argc, char **args, bool focus, bool copy) {
   fat_dir_t *entry = fat_parse_path(path, true);
   if(entry == NULL) {
      gui_writestr("Not found\n", 0);
      return -1;
   }
   uint8_t *prog = fat_read_file(entry->firstClusterNo, entry->fileSize);
   int task_index = elf_setup(regs, prog, entry->fileSize, argc, args, focus, !copy);
   if(task_index >= 0) {
      strcpy(gettasks()[task_index].process->exe_path, path);
   }
   free((uint32_t)prog, entry->fileSize);
   free((uint32_t)entry, sizeof(fat_dir_t));
   return task_index;
}

extern __attribute__((noreturn)) void kernel_panic(void);
void tasks_init(registers_t *regs) {
   // enable preemptive multitasking

   setSelectedWindowIndex(0);

   // launch idle process
   if(!tasks_launch_binary(regs, "/sys/progidle.bin")) {
      debug_printf("tasks_init: failed to launch idle program\n");
      kernel_panic(); // unable to init tasks at all
   }
   
   gui_get_windows()[tasks[current_task].process->window].minimised = true;
   gui_get_windows()[tasks[current_task].process->window].draw_func = NULL;
   strcpy(gui_get_windows()[tasks[current_task].process->window].title, "Idle Process");

   switching = true;
}

extern tss_t tss_start;
void switch_task(registers_t *regs) {
   if(!switching)
      return;

   int old_task = current_task;
   bool relaunched_idle = false;

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
         relaunched_idle = true;
         break;
      }
   } while(!tasks[current_task].enabled || tasks[current_task].paused);
   
   task_state_t *task = get_current_task_state();
   if(task->process->page_dir != page_get_current())
      swap_pagedir(task->process->page_dir);

   if(old_task != current_task) {
      tss_start.esp0 = tasks[current_task].kernel_stack_top;

      // save registers
      tasks[old_task].registers = *regs;

      // restore registers
      *regs = tasks[current_task].registers;
   }

   if(!relaunched_idle && !task->in_routine && task->process->event_queue_size > 0)
      task_execute_queued_subroutine(regs, current_task); // execute events from when task was paused
}

bool switch_to_task(int index, registers_t *regs) {
   //debug_printf("Switching from task %i to %i\n", current_task, index);
   if(index < 0 || index >= TOTAL_TASKS) {
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
      tss_start.esp0 = tasks[current_task].kernel_stack_top;

      // save registers
      tasks[old_task].registers = *regs;

      // restore registers
      *regs = tasks[current_task].registers;
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
         if(task->process->window < 0) continue;
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

void task_pause(task_state_t *task, task_pause_reason_t reason) {
   task->paused = true;
   task->pause_reason = reason;
}

void task_resume(task_state_t *task) {
   task->paused = false;
   task->pause_reason = PAUSE_NONE;
}

static bool task_unsnooze(task_state_t *task) {
   // wake from snooze
   if(!task->paused || task->pause_reason != PAUSE_SNOOZE) return false;
   task_resume(task);
   return true;
}

void task_wake(task_state_t *task) {
   // wake from snooze, or latch for the next snooze
   if(!task_unsnooze(task))
      task->wake_pending = true;
}

// index of the first queued event belonging to this thread, -1 if none
// queue is shared by every thread of process
int task_find_queued_subroutine(task_state_t *task) {
   for(int i = 0; i < task->process->event_queue_size; i++) {
      task_event_t *event = task->process->event_queue[i];
      if(!event) continue;
      if(event->task_id == task->task_id && event->task_uid == task->task_uid)
         return i;
   }
   return -1;
}

void task_remove_queued_subroutine(task_state_t *task, int index) {
   task->process->event_queue_size--;
   // todo: circular queue is faster than memmove
   memmove(&task->process->event_queue[index], &task->process->event_queue[index+1], (task->process->event_queue_size - index) * sizeof(task_event_t*));
}

bool task_execute_queued_subroutine(void *regs, int taskid) {
   // check events queue

   task_state_t *task = &tasks[taskid];
   if(task->in_routine)
      return false; // wait until task_subroutine_end to call this function

   while(true) {
      int e = task_find_queued_subroutine(task);
      if(e < 0) return false; // queue is empty

      task_event_t *event = task->process->event_queue[e];

      // skip stale msgs to avoid extra wakeups
      // if channel flags are set the notification isn't skipped
      bool stale = strequ(event->name, "msg") && event->args[0] == 0 && msg_notif_is_stale(task, event->args[2], event->args[1]);

      if(!stale) {
         if(!switch_to_task(taskid, regs)) return false;
         task_execute_subroutine(regs, event->name, event->addr, event->args, event->argc); // takes ownership of args
      } else {
         free((uint32_t)event->args, event->argc * sizeof(uint32_t*));
      }

      task_remove_queued_subroutine(task, e);
      msg_retry_notifications(task); // free'd queue entry so check for dropped messages

      free((uint32_t)event, sizeof(task_event_t));

      if(!stale) {
         task->routine_return_window = getSelectedWindowIndex();
         return true;
      }
   }
}

bool task_queue_subroutine(task_state_t *task, char *name, uint32_t addr, uint32_t *args, int argc) {
   if(!task->enabled) {
      debug_printf("Couldn't queue routine for task %i - task is disabled\n", task->task_id);
      free((uint32_t)args, sizeof(uint32_t*)*argc);
      return false;
   }

   task_unsnooze(task);

   // coalesce scroll and hover events
   if(strequ(name, "hover") || strequ(name, "scroll")) {
      for(int i = 0; i < task->process->event_queue_size; i++) {
         task_event_t *existing = task->process->event_queue[i];
         if(existing->task_id != task->task_id || existing->task_uid != task->task_uid) continue;
         if(existing->addr != addr) continue;
         if(!strequ(existing->name, name)) continue;

         if(strequ(name, "scroll") && existing->argc == 3 && argc == 3) {
            existing->args[2] += args[2]; // delta
            existing->args[1] = args[1];  // offset
            existing->args[0] = args[0];
            free((uint32_t)args, sizeof(uint32_t*)*argc);
         } else {
            // replace hover coords
            if(existing->args)
               free((uint32_t)existing->args, sizeof(uint32_t*)*existing->argc);
            existing->args = args;
            existing->argc = argc;
         }
         return true;
      }
   } else if(strequ(name, "msg")) {
      // coalesce msg events
      for(int i = 0; i < task->process->event_queue_size; i++) {
         task_event_t *existing = task->process->event_queue[i];
         if(existing->task_id != task->task_id || existing->task_uid != task->task_uid) continue;
         if(existing->addr != addr) continue;
         if(!strequ(existing->name, name)) continue;
         if(existing->args[2] != args[2] || existing->args[1] != args[1]) continue; // only coalesce msgs on same port & channel
         existing->args[0] |= args[0]; // OR together flags
         free((uint32_t)args, sizeof(uint32_t*)*argc);
         return true;
      }
   }

   if(task->process->event_queue_size == EVENT_QUEUE_SIZE) {
      debug_printf("Task %i hit maximum event queue size with event %s\n", task->task_id, name);
      free((uint32_t)args, sizeof(uint32_t*)*argc);
      return false;
   }
   // add to event queue
   task_event_t *event = (task_event_t*)malloc(sizeof(task_event_t));
   strcpy(event->name, name);
   event->addr = addr;
   event->args = args;
   event->argc = argc;
   event->task_id = task->task_id;
   event->task_uid = task->task_uid;
   task->process->event_queue[task->process->event_queue_size++] = event;

   return true;
}

bool task_call_subroutine(registers_t *regs, task_state_t *task, char *name, uint32_t addr, uint32_t *args, int argc) {

   // call subroutine immediately, switching to task
   
   if(!task->enabled) {
      debug_printf("Task %i is ended, exiting subroutine", task->task_id);
      free((uint32_t)args, sizeof(uint32_t*)*argc);
      return false;
   }
   
   task_unsnooze(task);

   if(task->paused || task->in_routine) {
      return task_queue_subroutine(task, name, addr, args, argc);
   }

   if(!switch_to_task(task->task_id, regs)) {
      free((uint32_t)args, sizeof(uint32_t*)*argc);
      return false;
   }

   // if the thread has no queued events, launch into routine immediately
   // otherwise just wait for queued event
   if(task_find_queued_subroutine(task) >= 0) {
      debug_printf("Not in routine but queue has content\n");
      bool queued = task_queue_subroutine(task, name, addr, args, argc);
      task_execute_queued_subroutine(regs, current_task);
      return queued;
   }

   task_execute_subroutine(regs, name, addr, args, argc);

   return true;
}

void task_subroutine_end(registers_t *regs) {
   // restore registers
   //debug_writestr("Ending subrouting\n");
   if(!tasks[current_task].in_routine)
      return;

   *regs = tasks[current_task].routine_return_regs;

   if(strequ(tasks[current_task].routine_name, "checkcmd"))
      cleanup_checkcmd_args(tasks[current_task].process, tasks[current_task].routine_args);

   free((uint32_t)tasks[current_task].routine_args, tasks[current_task].routine_argc*sizeof(uint32_t*));
   tasks[current_task].routine_args = NULL;
   tasks[current_task].routine_argc = 0;

   tasks[current_task].in_routine = false;

   // check for any other queued events and run if there are
   task_execute_queued_subroutine(regs, current_task);

   // queue notifications that were dropped while event queue was full
   msg_retry_notifications(&tasks[current_task]);

   if(tasks[current_task].routine_return_window >= 0)
      setSelectedWindowIndex(tasks[current_task].routine_return_window);

   if(tasks[current_task].paused) {
      switch_task(regs); // yield
   }
}

void task_write_to_window(int task, char *out, bool children) {
   task_state_t *t = &tasks[task];
   // write to task main window if it exists, otherwise write to fd 1
   int w = t->process->window;
   if(w >= 0 && w < getWindowCount() && !getWindow(w)->closed) {
      gui_window_t *window = getWindow(w);
      window_writestr(out, window->txtcolour, w);
      if(children) {
         for(int i = 0; i < window->child_count; i++) {
            window_writestr(out, window->txtcolour, get_window_index_from_pointer(window->children[i]));
         }
      }
   } else {
      fs_file_t *fd1 = t->process->file_descriptors[1];
      if(!fd1) {
         debug_printf("task_write_to_window: no fd1 or main window\n");
         return;
      }
      int r = fs_write(fd1, (uint8_t*)out, strlen(out), task);
      if(r > 0 && fd1->type == FS_TYPE_PIPE && fd1->pipe)
         fs_pipe_wake_reader(fd1->pipe);
   }
}

static void double_fault_handler() {
   window_writestr("Double fault exception, hanging kernel\n", COLOUR_RED, 0);
   debug_printf("eip: 0x%h / esp: 0x%h / task: %i", tss_start.eip, tss_start.esp, current_task);
   if(current_task >= 0 && current_task < TOTAL_TASKS) {
      task_state_t *task = &gettasks()[current_task];
      if(task->in_syscall)
         debug_printf(" / syscall: %i", task->syscall_no);
      debug_printf("\nTask kstack: 0x%h-0x%h\n", task->kernel_stack_top - KSTACK_SIZE + 0x1000, task->kernel_stack_top);
      if(tss_start.esp >= task->kernel_stack_top - KSTACK_SIZE && tss_start.esp <= task->kernel_stack_top - KSTACK_SIZE + 0x1000) {
         // inside stack guard
         window_writestr("kernel stack overflow\n", COLOUR_RED, 0);
      }
   }
   kernel_panic();
   while(true) {}
}

void tss_init() {
   // setup tss entry in gdt
   extern gdt_entry_t gdt_tss;
   extern tss_t tss_start;

   uint32_t base = (uint32_t)&tss_start;
   uint32_t limit = sizeof(tss_t) - 1;
   uint8_t gran = 0x00;

   gdt_tss.base_low = (base & 0xFFFF);
	gdt_tss.base_middle = (base >> 16) & 0xFF;
	gdt_tss.base_high = (base >> 24) & 0xFF;

   gdt_tss.limit_low = (limit & 0xFFFF);
	gdt_tss.granularity = ((limit >> 16) & 0x0F);
   gdt_tss.granularity |= (gran & 0xF0);
}

void tss_df_init() {
   // setup double fault tss (uses separate kstack)
   extern tss_t df_tss_start;
   uint32_t base = (uint32_t)&df_tss_start;
   uint32_t limit = sizeof(tss_t) - 1;
   uint8_t gran = 0x00;
   extern gdt_entry_t gdt_df_tss;
   
   gdt_df_tss.base_low = (base & 0xFFFF);
	gdt_df_tss.base_middle = (base >> 16) & 0xFF;
	gdt_df_tss.base_high = (base >> 24) & 0xFF;

   gdt_df_tss.limit_low = (limit & 0xFFFF);
	gdt_df_tss.granularity = ((limit >> 16) & 0x0F);
   gdt_df_tss.granularity |= (gran & 0xF0);

   df_tss_start.eip = (uint32_t)&double_fault_handler;
}

// launch/string args are constructed by the kernel in two places: api_launch_task, endtask_debug
void free_launch_args(char **args, int argc) {
   if(!args) return;
   for(int i = 0; i < argc; i++) {
      if(args[i])
         free((uint32_t)args[i], strlen(args[i])+1);
   }
   free((uint32_t)args, sizeof(char*) * (argc+1)); // note alloc includes trailing null str pointer
}

int current_servicing_task = -1;

static bool task_addr_demand_paged(task_state_t *task, uint32_t addr) {
   // check if addr is within demand paged regions (currently heap only)
   process_t *process = task->process;
   return addr >= process->heap_start && addr < process->heap_end;
}

int task_validate_str(task_state_t *task, char *str, int maxlen) {
   // validate str provided by user before kernel uses/reads it
   // out: len on success, -1 on invalid
   if(maxlen < 0) return -1;
   if(maxlen == 0) return 0;
   uint32_t vaddr = (uint32_t)str;
   page_dir_entry_t *page_dir = task->process->page_dir;

   // check mapping
   uint32_t base = vaddr & ~PAGE_MASK;
   int maxpages = (maxlen+(vaddr&PAGE_MASK)+(PAGE_SIZE-1))/PAGE_SIZE;
   for(int i = 0; i < maxpages; i++) {
      uint32_t page_addr = base + i*PAGE_SIZE;
      if(page_checkmapping(page_dir, page_addr) < PAGE_USERREAD) {
         maxpages = i;
         break;
      }
   }
   // nothing mapped at addr
   if(maxpages == 0) return -1;

   int mappedsize = maxpages*PAGE_SIZE - (vaddr&PAGE_MASK);
   int limit = mappedsize < maxlen ? mappedsize : maxlen;
   int len = strnlen((char*)vaddr, limit);
   if(len == limit)
      return limit < maxlen ? -1 : -2; // -1 ran out of mapping, -2 mapped without NULL in maxlen
   return len;
}

bool task_validate_mem(task_state_t *task, void *mem, int len, bool rw) {
   // check size len is mapped to user at addr
   if(len < 0) return false;
   if(len == 0) return true;
   uint32_t vaddr = (uint32_t)mem;
   page_dir_entry_t *page_dir = task->process->page_dir;

   int check = rw ? PAGE_USERRW : PAGE_USERREAD;
   // check mapping
   uint32_t base = vaddr & ~PAGE_MASK;
   int maxpages = (len+(vaddr&PAGE_MASK)+(PAGE_SIZE-1))/PAGE_SIZE;
   for(int i = 0; i < maxpages; i++) {
      uint32_t page_addr = base + i*PAGE_SIZE;
      if(page_checkmapping(page_dir, page_addr) < check
      && !(rw && task_addr_demand_paged(task, page_addr))) { // for writes allow lazy mapped mem - reads aren't allowed to allocate memory
         return false;
      }
   }
   return true;
}

int task_validate_maxsize(task_state_t *task, void *mem, int max, bool rw) {
   // get maximum mapped size of user provided area up to max - allows for partial read/writes
   if(max < 0) return -1;
   if(max == 0) return 0;
   uint32_t vaddr = (uint32_t)mem;
   page_dir_entry_t *page_dir = task->process->page_dir;

   int check = rw ? PAGE_USERRW : PAGE_USERREAD;
   // check mapping
   uint32_t base = vaddr & ~PAGE_MASK;
   int maxpages = (max+(vaddr&PAGE_MASK)+(PAGE_SIZE-1))/PAGE_SIZE;
   for(int i = 0; i < maxpages; i++) {
      uint32_t page_addr = base + i*PAGE_SIZE;
      if(page_checkmapping(page_dir, page_addr) < check
      && !(rw && task_addr_demand_paged(task, page_addr))) { // for writes allow lazy mapped mem - reads aren't allowed to allocate memory
         maxpages = i;
         break;
      }
   }
   if(maxpages == 0) return 0;
   int maxsize = maxpages*PAGE_SIZE - (vaddr&PAGE_MASK);
   if(maxsize < max)
      max = maxsize;
   return max;
}

// copy up to size bytes to task - checks mapped size
int copy_to_task(int task, void *dest, void *src, size_t size) {
   task_state_t *task_state = &gettasks()[task];
   int maxsize = task_validate_maxsize(task_state, dest, size, 1);
   if(maxsize < 0 || (size > 0 && maxsize == 0)) return -1; // invalid buffer/size

   int prev = current_servicing_task;
   current_servicing_task = task;
   page_dir_entry_t *old_dir = page_get_current();
   page_dir_entry_t *task_dir = task_state->process->page_dir;
   bool swapped = task_dir != old_dir;
   if(swapped) swap_pagedir(task_dir);
   memcpy(dest, src, maxsize);
   if(swapped) swap_pagedir(old_dir);
   current_servicing_task = prev;
   return maxsize;
}

// copy up to size bytes from task - checks mapped size
int copy_from_task(int task, void *dest, void *src, size_t size) {
   task_state_t *task_state = &gettasks()[task];
   int maxsize = task_validate_maxsize(task_state, src, size, 0);
   if(maxsize < 0 || (size > 0 && maxsize == 0)) return -1; // invalid buffer/size

   int prev = current_servicing_task;
   current_servicing_task = task;
   page_dir_entry_t *old_dir = page_get_current();
   page_dir_entry_t *task_dir = task_state->process->page_dir;
   bool swapped = task_dir != old_dir;
   if(swapped) swap_pagedir(task_dir);
   memcpy(dest, src, maxsize);
   if(swapped) swap_pagedir(old_dir);
   current_servicing_task = prev;
   return maxsize;
}
