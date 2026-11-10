#include "tasks.h"
#include "window.h"
#include "windowmgr.h"
#include "paging.h"
#include "events.h"
#include "fs.h"
#include "shared.h"
#include "msg.h"
#include "ksync.h"
#include "cpu.h"

task_state_t *tasks;
int current_task = -1;
bool switching = false; // preemptive multitasking

uint32_t process_uid_counter = 0;
uint32_t task_uid_counter = 0;
uint32_t task_event_counter = 0; // incremented on start/end

process_t *create_process(uint32_t entry, uint32_t size, bool privileged) {
   process_t *process = kmalloc(sizeof(process_t));
   if(!process) return NULL;
   process->uid = process_uid_counter++;
   process->prog_entry = entry;
   process->prog_start = entry;
   process->prog_size = size;
   process->privileged = privileged;
   process->vmem_start = 0;
   process->vmem_end = 0;
   process->page_dir = page_get_kernel_pagedir(); // default to kernel pagedir
   process->window = -1;
   process->no_allocated = 0;
   for(int i = 0; i < 3; i++)
      process->stdio[i] = -1;
   for(int i = 0; i < PROCESS_MAX_KOBJ; i++)
      process->kobj[i] = NULL;
   process->kobj_count = 0;
   process->port_count = 0;
   strcpy(process->working_dir, "/sys");
   strcpy(process->exe_path, "");
   process->no_threads = 0;
   for(int i = 0; i < MAX_TASK_THREADS; i++)
      process->threads[i] = NULL;
   process->heap_start = 0;
   process->heap_end = 0;
   process->launch_argc = 0;
   process->launch_args = NULL;
   process->regions = NULL;
   
   return process;
}

static bool create_task_stacks(task_state_t *task) {
   // lazy allocated stacks
   if(!task->stack_base) {
      void *stack = kmalloc(TASK_STACK_SIZE - 0x1000); // don't need to allocate physical memory for page guard
      if(!stack) {
         debug_printf("couldn't allocate stack for task %i\n", task->task_id);
         return false;
      }
      task->stack_base = (uint32_t)stack;
   }
   if(!task->kernel_stack_top) {
      void *kernel_stack = kmalloc(KSTACK_SIZE); // do need to allocate physical memory for page guard (currently)
      if(!kernel_stack) {
         debug_printf("couldn't allocate kernel stack for task %i\n", task->task_id);
         return false;
      }
      task->kernel_stack_top = (uint32_t)kernel_stack + KSTACK_SIZE;
   }
   return true;
}

// returns thread no
int create_task_entry(int index, uint32_t entry, uint32_t size, bool privileged, process_t *process) {
   if(!create_task_stacks(&tasks[index]))
      return -1;
   memset((void*)tasks[index].stack_base, 0, TASK_STACK_SIZE - 0x1000); // clear stack

   kqueue_init(&tasks[index].event_queue, &tasks[index].event_buf, EVENT_QUEUE_SIZE, sizeof(task_event_t));

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
   tasks[index].kill_pending = false;
   tasks[index].routine = E_NONE;
   tasks[index].checkcmd_buffer = NULL;
   tasks[index].in_syscall = false;
   tasks[index].msg_func = NULL;
   tasks[index].msg_channel_count = 0;
   tasks[index].msg_notify_pending = false;
   tasks[index].mutex = NULL;
   tasks[index].mutex_next_waiter = NULL;
   tasks[index].owned_mutexes = NULL;
   tasks[index].kernel_esp = 0;
   tasks[index].ticks = 0;
   
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

bool setup_task_init(int index, bool focus, bool minimised) {
   task_state_t *task = &tasks[index];

   // note: tasks created here are always main thread
   if(!task_map_stack(task, 0))
      return false;

   task->registers.ds = USR_DATA_SEG | 3;
   task->registers.cs = USR_CODE_SEG | 3;
   task->registers.eflags = 0x202; // interrupts enabled
   task->registers.ss = USR_DATA_SEG | 3;

   int window = windowmgr_add();
   if(window < 0)
      return false;
   
   process_t *process = task->process;
   process->window = window;

   if(focus) {
      windowmgr_focus_swap(NULL);
      setSelectedWindowIndex(window);
      window_draw_outline(getWindow(window), false);
   }

   if(minimised)
      getWindow(process->window)->minimised = true;

   task_event_counter++;
   return true;
}

bool launch_task(int index, registers_t *regs, bool focus) {
   int old_task = current_task;
   current_task = index;

   if(!setup_task_init(index, focus, false)) {
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
   tasks[index].reserved = false;
   process_t *process = tasks[index].process;
   if(!process) return;

   if(process->prog_size != 0)
      kfree((void*)process->prog_start);
   if(process->page_dir != page_get_kernel_pagedir())
      free_page_dir(process->page_dir);
   kfree(process);
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
      if(!tasks[i].enabled && !tasks[i].reserved && i != current_task) {
         tasks[i].reserved = true;
         return i;
      }
      
   return -1;
}

void task_unreserve(int task) {
   if(task < 0 || task >= TOTAL_TASKS) return;
   tasks[task].reserved = false;
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
      if(regs != NULL) switch_task(regs, false);
}

void task_reset_windows(int task) {
   // remove funcs from all associated windows
   int taskw = tasks[task].process->window;
   if(taskw >= 0 && taskw < getWindowCount() && !getWindow(taskw)->closed) {
      gui_window_t *window = getWindow(taskw);

      if(window->scrollbar) {
         window->scrollbar->visible = false;
         window->scrollbar->return_func = NULL;
      }
      if(window->read_task >= 0) // notify window read_task of eof (mirrors window_close)
         fs_read_window_callback(window, true);
      window_resetfuncs(window);
      strcpy(window->title, "KTerm");
      window_draw_outline(window, true);
      for(int i = 0; i < window->child_count; i++) {
         gui_window_t *child = (gui_window_t*)window->children[i];
         if(!child || child->closed) continue;
         if(child->read_task >= 0)
            fs_read_window_callback(child, true);
         window_resetfuncs(child);
         strcpy(child->title, "KTerm");
         window_draw_outline(child, true);
      }
   }
}

// heap page returned back to kernel only access (set in new_page)
void unshare_heap_page(page_dir_entry_t *dir, uint32_t addr) {
   addr = page_align_down(addr);
   map(dir, addr, addr, 0, 0, 0);
}

// hacky but less hacky than having task clean this up
static void cleanup_checkcmd_args(process_t *process, char *buffer) {
   if(process)
      unshare_heap_page(process->page_dir, (uint32_t)buffer);
   kfree(buffer);
}

static void free_event_queue(task_state_t *task) {
   task_event_t *event;
   while((event = kqueue_peek(&task->event_queue))) {
      if(event->type == E_CHECKCMD)
         cleanup_checkcmd_args(task->process, (char*)event->args[0]);
      kqueue_drop(&task->event_queue);
   }
}

static bool end_task_helper(int index, registers_t *regs, bool child) {
   if(index < 0 || index >= TOTAL_TASKS) return false;
   if(index == 0) {
      debug_printf("Can't kill idle process\n");
      return false;
   }
 
   task_state_t *task = &tasks[index];
   if(!task->enabled) {
      debug_printf("Task %u already ended\n", index);
      return true;
   }

   if(!regs && !child && get_current_task() == index) {
      // note: freeing child tasks/threads doesn't pass regs
      debug_printf("end_task called without regs on current task\n");
      kernel_panic();
   }

   if(task->kernel_esp) {
      if(is_kthread(task))
         debug_printf("Can't end kthread %i\n", index);
      else
         task->kill_pending = true; // usermode parked in kernel, wait until end of syscall
      return false;
   }

   if(task->process->window >= 0)
      task_write_to_window(index, "<Task ended>\n", true);

   if(task->routine != E_NONE) {
      debug_printf("Task was in routine %i\n", task->routine);
      if(task->routine == E_CHECKCMD)
         cleanup_checkcmd_args(task->process, task->checkcmd_buffer);
      task->checkcmd_buffer = NULL;
   }

   if(task->in_syscall)
      debug_printf("Task was in syscall %i\n", task->syscall_no);

   task->enabled = false;

   // free channels
   msg_cleanup_task(task);

   // release held mutexes, remove task from any waiter queues
   ksync_cleanup_task(task);

   // free task event queue
   free_event_queue(task);

   if(task->process->threads[0] == task) {
      // main thread, terminate entire process

      // kill other threads
      int remaining_threads = 0;
      if(task->process->no_threads - 1 > 0)
         debug_printf("Ending %i other threads of process\n", task->process->no_threads - 1);
      for(int i = 1; i < MAX_TASK_THREADS; i++) {
         task_state_t *thread = task->process->threads[i];
         if(!thread) continue;
         if(!end_task_helper(thread->task_id, NULL, true)) {
            task->process->threads[i] = NULL;
            task->process->threads[remaining_threads++] = thread;
         }
         else
            thread->process = NULL;
      }

      if(!remaining_threads) {
         // free task memory
         if(task->process->prog_size != 0)
            kfree((void*)task->process->prog_start);

         task_reset_windows(index);

         // free heap
         if(task->process->heap_end > task->process->heap_start) {
            for(uint32_t addr = task->process->heap_start; addr < task->process->heap_end; addr+=PAGE_SIZE) {
               uint32_t phys = page_getphysical(task->process->page_dir, addr);
               if(phys != (uint32_t)-1)
                  kfree((void*)phys);
            }
         }

         // free kernel objs
         process_release_kobjs(task->process);

         // free ports
         msg_cleanup_process(task->process);

         // switch pagedir before freeing
         if(task->process->page_dir == page_get_current()) {
            if(regs)
               switch_task(regs, false); // all process threads are ended
            else
               swap_pagedir(page_get_kernel_pagedir());
         }

         // free page dir
         if(task->process->page_dir != page_get_kernel_pagedir())
            free_page_dir(task->process->page_dir);
         else // binary task - page dir persists, so undo stack mapping
            task_unmap_stack(task);

         // free launch args
         free_launch_args(task->process->launch_args, task->process->launch_argc);

         process_free_vm(task->process);

         // free process
         kfree(task->process);
      } else {
         debug_printf("%i threads couldn't be ended\n", remaining_threads);
      }
      task->process = NULL;
   } else {
      task_unmap_stack(task);

      // reclaim slot from process threads
      for(int i = 0; i < MAX_TASK_THREADS; i++) {
         if(task->process->threads[i] != task) continue;
         task->process->threads[i] = NULL;
         task->process->no_threads--;
         break;
      }
   }

   task->reserved = false;
   task->routine = E_NONE;

   if(regs && !get_current_task_state()->enabled)
      switch_task(regs, false);

   task_event_counter++;

   return true;
}

bool end_task(int index, registers_t *regs) {
   return end_task_helper(index, regs, false);
}

void tasks_alloc() {
   tasks = kmalloc(sizeof(task_state_t) * TOTAL_TASKS);
   for(int i = 0; i < TOTAL_TASKS; i++) {
      tasks[i].enabled = false;
      tasks[i].reserved = false;
      tasks[i].process = NULL;
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
   int fsize;
   uint8_t *prog = fs_read_file_kernel(path, &fsize);
   if(!prog) {
      gui_writestr("Program not found\n", 0);
      task_unreserve(index);
      return false;
   }
   if(create_task_entry(index, (uint32_t)prog, fsize, false, NULL) == -1) {
      debug_printf("tasks_launch_binary: create task entry failed\n");
      kfree(prog);
      task_unreserve(index);
      return false;
   }
   if(regs && !launch_task(index, regs, false)) {
      debug_printf("tasks_launch_binary: launch failed\n");
      task_discard_entry(index); // frees prog
      return false;
   } else if(!regs) {
      // setup but don't launch
      if(!setup_task_init(index, true, false)) {
         debug_printf("tasks_launch_binary: setup failed\n");
         task_discard_entry(index);
         return false;
      }
      tasks[index].enabled = true;
   }
   gui_redrawall();
   return true;
}

// doesn't immediately switch, caller is responsible for enabling the task
int tasks_setup_elf(char *path, int argc, char **args, bool focus, bool minimised) {
   int fsize;
   uint8_t *prog = fs_read_file_kernel(path, &fsize);
   if(!prog) {
      gui_writestr("Not found\n", 0);
      return -1;
   }
   int task_index = elf_setup(prog, fsize, argc, args, focus, minimised);
   if(task_index >= 0) {
      process_t *process = gettasks()[task_index].process;
      strcpy(process->exe_path, path);
   }
   kfree(prog);
   return task_index;
}

// abort & cleanup tasks_setup_elf
void task_abort_setup(int index) {
   tasks[index].enabled = true; // must be enabled for end_task
   end_task(index, NULL);
}

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

static inline bool task_resumable(task_state_t *task, bool resume_kernel) {
   return task->enabled && !task->paused && (resume_kernel || !task->kernel_esp);
}

// returns -1 for none available
int next_task(bool resume_kernel) {
   // find next enabled task (round robin)
   int task = current_task;
   do {
      task++;
      task%=TOTAL_TASKS;

      if(task == current_task && !task_resumable(&tasks[task], resume_kernel)) {
         // no tasks
         return -1;
      }
   } while(!task_resumable(&tasks[task], resume_kernel));
   return task;
}

extern tss_t tss_start;
extern void resume_kernel(uint32_t esp);

void switch_task(registers_t *regs, bool resume) {
   if(!switching)
      return;

   int old_task = current_task;

   current_task = next_task(resume);
   if(current_task == -1) {
      // no tasks, launch idle process
      debug_printf("No tasks found\n");
      // save registers
      tasks[old_task].registers = *regs;
      tasks_init(regs);
      old_task = current_task; // launch_task already swapped regs/pagedir/esp0
   }

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

   if(tasks[current_task].kernel_esp) {
      uint32_t esp = tasks[current_task].kernel_esp;
      tasks[current_task].kernel_esp = 0;
      resume_kernel(esp); // yield to instead? (allow resume)
      return; // never hit
   }
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
   if(tasks[index].kernel_esp) {
      debug_printf("Task switch failed: task %i is in kernel\n", index);
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

static bool can_park(void) {
   // check that current_task is in syscall (ie not irq)
   // sanity check current kstack matches current_task's kstack
   if(!switching || current_task < 0 || !tasks[current_task].in_syscall)
      return false;
   uint32_t esp = read_esp();
   uint32_t top = tasks[current_task].kernel_stack_top;
   return esp > top - KSTACK_SIZE && esp <= top;
}

extern void yield_to_user(registers_t *regs, uint32_t *old_esp);
extern void yield_to_kernel(uint32_t esp, uint32_t *old_esp);

// yield to specific task
bool kernel_yield_to(int next_index) {
   if(!can_park()) return false;
   task_state_t *cur = get_current_task_state();
   if(next_index < 0 || next_index >= TOTAL_TASKS || next_index == cur->task_id) return false;
   if(!task_resumable(&tasks[next_index], true)) return false;

   task_state_t *next = &tasks[next_index];

   current_task = next_index;
   tss_start.esp0 = next->kernel_stack_top;
   
   if(next->process->page_dir != page_get_current())
      swap_pagedir(next->process->page_dir);
   
   if(!next->kernel_esp) {
      // run any routines queued while task was parked
      task_execute_queued_subroutine(&next->registers, next_index);
      if(next->crashed) { // execute_subroutine failed, task crashed
         // undo swap
         current_task = cur->task_id;
         tss_start.esp0 = cur->kernel_stack_top;
         if(cur->process->page_dir != page_get_current())
            swap_pagedir(cur->process->page_dir);
         return false; // no yield
      }

      // copy next tasks registers into stack
      registers_t *frame = (registers_t*)(next->kernel_stack_top - sizeof(registers_t));
      *frame = next->registers;
      // iret into it immediately
      yield_to_user(frame, &cur->kernel_esp);
   } else {
      uint32_t esp = next->kernel_esp;
      next->kernel_esp = 0;
      yield_to_kernel(esp, &cur->kernel_esp);
   }

   return true;
}

bool kernel_yield() {
   int next = next_task(true);
   while(next >= 0) {
      if(!kernel_yield_to(next)) {
         int next2 = next_task(true);
         if(next == next2) return false;
         next = next2;
         continue; // may have crashed while attempting to resume/execute queued routine
      }
      return true;
   }
   return false;
}

void kernel_block() {
   if(!kernel_yield()) {
      debug_printf("kernel_block failed\n"); // i.e. no other tasks available
      kernel_panic();
   }
}

bool kernel_yield_if_blocking() {
   if(!can_park()) return false;
   
   // check for pending interrupt
   outb(0x20, 0x0A); // read pic irr
   if(!(inb(0x20) & 1))
      return false;

   return kernel_yield();
}

bool kernel_exit_resume(registers_t *regs, int task) {
   // used to exit an irq and run a kthread
   // eoi must already be sent
   if(!(regs->cs & 3) || current_task < 0) return false;
   if(!tasks[task].enabled || tasks[task].paused || !tasks[task].kernel_esp)
      return false;
      
   if(current_task >= 0) {
      // save registers
      tasks[current_task].registers = *regs;
   }

   current_task = task;
   if(tasks[task].process->page_dir != page_get_current())
      swap_pagedir(tasks[task].process->page_dir);

   tss_start.esp0 = tasks[task].kernel_stack_top;

   uint32_t esp = tasks[task].kernel_esp;
   tasks[task].kernel_esp = 0;
   resume_kernel(esp);
   return true; // never hit
}

void end_kthread(int task) {
   if(!tasks[task].in_syscall) return;
   tasks[task].enabled = false;
   tasks[task].reserved = false;
   if(tasks[task].process) {
      kfree(tasks[task].process);
      tasks[task].process = NULL;
   }
   if(task == current_task) {
      if(!kernel_yield()) {
         debug_printf("end_kthread failed\n");
         kernel_panic();
      }
   }
}

void kthread_exit() {
   end_kthread(current_task);
   kernel_panic();
}

bool launch_kthread(void *func, char *name, bool run) {
   int index = get_free_task_index();
   if(index == -1) {
      debug_printf("No free tasks\n");
      return false;
   }

   // create task + process entry
   // kthreads use kernel page dir
   if(create_task_entry(index, 0, 0, true, NULL) < 0) {
      task_unreserve(index);
      return false;
   }
   
   task_state_t *task = &gettasks()[index];
   task->in_syscall = true; // kthreads are always in kernel/'syscall' (ie not in irq)
   strcpy(task->process->exe_path, name);

   // setup stack (see resume_kernel)
   // ebx, esi, edi, ebp start at 0
   memset((void*)(task->kernel_stack_top - sizeof(uint32_t)*6), 0, sizeof(uint32_t)*4);
   ((uint32_t*)task->kernel_stack_top)[-2] = (uint32_t)func; // ret into this function
   ((uint32_t*)task->kernel_stack_top)[-1] = (uint32_t)kthread_exit; // called when func rets

   task->kernel_esp = task->kernel_stack_top - sizeof(uint32_t)*6;
   task->enabled = true;

   // launch into it immediately... or just let scheduler do its work
   if(run)
      kernel_yield_to(index);
   return true;
}

bool is_kthread(task_state_t *task) {
   return task->in_syscall && task->process && !task->process->prog_start && task->process->page_dir == page_get_kernel_pagedir();
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
   if(windowIndex < 0) return -1;
   for(int i = 0; i < TOTAL_TASKS; i++) {
      task_state_t *task = &tasks[i];
      if(!task->enabled) continue;
      if(task->process->window == windowIndex) {
         return task->process->threads[0] ? task->process->threads[0]->task_id : i; // main thread
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
               return task->process->threads[0] ? task->process->threads[0]->task_id : i; // main thread
         }
      }
   }
   return -1;
}

bool task_execute_subroutine(registers_t *regs, task_event_type_t type, uint32_t addr, uint32_t *args, int argc) {
   if(argc > TASK_EVENT_MAX_ARGS) {
      debug_printf("task_execute_subroutine called with too many args\n");
      kernel_panic();
   }

   // construct stack frame
   uint32_t frame[TASK_EVENT_MAX_ARGS+1];

   // push unused return address to stack
   frame[0] = regs->eip;

   // push arguments to stack
   for(int i = 0; i < argc; i++)
      frame[i+1] = args[i];

   int size = 4*(argc+1);
   if(copy_to_task(current_task, (void*)(regs->useresp - size), (void*)frame, size) != size) {
      debug_printf("task_execute_subroutine: copying args failed\n");
      return false;
   }
   
   tasks[current_task].routine = type;
   tasks[current_task].checkcmd_buffer = type == E_CHECKCMD ? (void*)args[0] : NULL;

   // save registers

   tasks[current_task].routine_return_regs = *regs;

   regs->useresp -= size;

   // simulate JMP

   // update eip to func addr
   regs->eip = addr;

   return true;
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

bool task_execute_queued_subroutine(void *regs, int taskid) {
   // check events queue

   task_state_t *task = &tasks[taskid];
   // wait until task_subroutine_end to call this function if in routine
   if(task->routine != E_NONE || task->crashed)
      return false;

   while(true) {
      task_event_t *event = kqueue_peek(&task->event_queue);
      if(!event)
         return false; // queue is empty

      // skip stale msgs to avoid extra wakeups
      // if channel flags are set the notification isn't skipped
      bool stale = event->type == E_MSG && event->args[2] == 0 && msg_notif_is_stale(task, event->args[0], event->args[1]);

      if(!stale) {
         if(!switch_to_task(taskid, regs)) return false;
         if(!task_execute_subroutine(regs, event->type, event->addr, event->args, event->argc)) {
            crash_task(14, NULL, taskid);
            return false;
         }
      }

      kqueue_drop(&task->event_queue);
      msg_retry_task_notifications(task); // free'd queue entry so check for dropped messages

      if(!stale) return true;
   }
}

bool task_queue_subroutine(task_state_t *task, task_event_type_t type, uint32_t addr, uint32_t *args, int argc) {
   if(!task->enabled) {
      debug_printf("Couldn't queue routine for task %i - task is disabled\n", task->task_id);
      return false;
   }

   if(argc > TASK_EVENT_MAX_ARGS) {
      debug_printf("task_queue_subroutine: exceeded event max args\n");
      return false;
   }

   task_unsnooze(task);

   // coalesce scroll and hover events
   if(type == E_HOVER || type == E_SCROLL || type == E_DRAGGED) {
      // check last event
      task_event_t *existing = kqueue_last(&task->event_queue);
      if(existing && existing->addr == addr && existing->type == type
      && existing->args[argc-1] == args[argc-1]) { // only batch for same window
         if(type == E_SCROLL && existing->argc == 3 && argc == 3) {
            existing->args[0] += args[0]; // delta
            existing->args[1] = args[1];  // offset
            existing->args[2] = args[2];
         } else {
            // replace hover/drag coords
            memcpy(existing->args, args, sizeof(uint32_t)*argc);
            existing->argc = argc;
         }
         return true;
      }
   } else if(type == E_MSG) {
      // coalesce msg events
      for(int i = 0; i < kqueue_count(&task->event_queue); i++) {
         task_event_t *existing = kqueue_at(&task->event_queue, i);
         if(existing->addr != addr) continue;
         if(existing->type != type) continue;
         if(existing->args[0] != args[0] || existing->args[1] != args[1]) continue; // only coalesce msgs on same port & channel
         existing->args[2] |= args[2]; // OR together flags
         return true;
      }
   }

   
   // add to event queue
   task_event_t *event = kqueue_reserve(&task->event_queue);
   if(!event) {
      debug_printf("Task %i hit maximum event queue size with event type %i\n", task->task_id, type);
      return false;
   }
   event->type = type;
   event->addr = addr;
   memcpy(event->args, args, sizeof(uint32_t)*argc);
   event->argc = argc;

   return true;
}

bool task_call_subroutine(registers_t *regs, task_state_t *task, task_event_type_t type, uint32_t addr, uint32_t *args, int argc) {

   // call subroutine immediately, switching to task
   
   if(!task->enabled) {
      debug_printf("Task %i is ended, exiting subroutine", task->task_id);
      return false;
   }
   
   task_unsnooze(task);

   if(task->paused || task->routine != E_NONE || task->kernel_esp) {
      return task_queue_subroutine(task, type, addr, args, argc);
   }

   if(!switch_to_task(task->task_id, regs)) {
      return false;
   }

   // if the thread has no queued events, launch into routine immediately
   // otherwise just wait for queued event
   task_event_t *event = kqueue_peek(&task->event_queue);
   if(event) {
      debug_printf("Not in routine but queue has content\n");
      bool queued = task_queue_subroutine(task, type, addr, args, argc);
      if(!task_execute_queued_subroutine(regs, current_task) && task->crashed)
         switch_task(regs, false);
      return queued;
   }

   if(!task_execute_subroutine(regs, type, addr, args, argc)) {
      crash_task(14, regs, task->task_id);
      return false;
   }

   return true;
}

void task_subroutine_end(registers_t *regs) {
   // restore registers
   if(tasks[current_task].routine == E_NONE)
      return;

   *regs = tasks[current_task].routine_return_regs;

   if(tasks[current_task].routine == E_CHECKCMD)
      cleanup_checkcmd_args(tasks[current_task].process, tasks[current_task].checkcmd_buffer);
   tasks[current_task].checkcmd_buffer = NULL;

   tasks[current_task].routine = E_NONE;

   // check for any other queued events and run if there are
   task_execute_queued_subroutine(regs, current_task);

   // queue notifications that were dropped while event queue was full
   msg_retry_task_notifications(&tasks[current_task]);

   if(tasks[current_task].paused) {
      switch_task(regs, false); // yield
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
            if(!window->children[i] || ((gui_window_t*)window->children[i])->closed)
               continue;
            window_writestr(out, window->txtcolour, get_window_index_from_pointer(window->children[i]));
         }
      }
   } else {
      // write to fd/handle 1 (stdout)
      kobj_handle_t fd1 = get_process_stdio(t->process, 1); // opens if not already
      kobj_ref_t *ref = fd1 >= 0 ? process_get_kobj_type(t->process, fd1, KOBJ_FILE) : NULL;
      if(!ref) {
         debug_printf("task_write_to_window: no fd1 or main window\n");
         return;
      }
      fs_file_t *file = ref->inst->data;
      if(file->type == FS_TYPE_FILE && !get_current_task_state()->in_syscall) {
         debug_printf("task_write_to_window: can't write to file from irq\n"); // write to file may block
         return;
      }
      int r = fs_write(file, (uint8_t*)out, strlen(out), -1);
      if(r > 0 && file->type == FS_TYPE_PIPE && file->pipe)
         fs_pipe_wake_reader(file->pipe);
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
         kfree(args[i]);
   }
   kfree(args);
}

int current_servicing_task = -1;

bool task_addr_demand_paged(task_state_t *task, uint32_t addr) {
   // check if addr is within demand paged regions (currently heap only)
   process_t *process = task->process;
   return addr >= process->heap_start && addr < process->heap_end;
}

bool task_demand_map(process_t *process, uint32_t addr) {
   // allocate and map a zeroed page for a demand paged addr
   addr = addr & ~PAGE_MASK;  // page align
   int mapping = page_checkmapping(process->page_dir, addr);
   if(mapping == PAGE_USERRW) return true; // already mapped
   if(mapping != PAGE_NOTPRESENT) { // avoid overwriting mappings
      debug_printf("task_demand_map failed: 0x%h mapped type %i\n", addr, mapping);
      return false;
   }
   uint8_t *page = kmalloc(PAGE_SIZE); // returns physical addr
   if(!page) {
      debug_printf("demand paging: system ran out of physical memory\n");
      return false;
   }
   memset(page, 0, PAGE_SIZE); // zero mem

   if(!map(process->page_dir, (uint32_t)page, addr, 1, 1, 0)) {
      debug_printf("demand paging: couldn't map 0x%h\n", addr);
      kfree(page);
      return false;
   }
   invlpg(addr);
   return true;
}

static int task_prefault(task_state_t *task, void *mem, int size) {
   // map lazy allocated pages to avoid fauling mid operation
   uint32_t vaddr = (uint32_t)mem;
   page_dir_entry_t *page_dir = task->process->page_dir;

   uint32_t base = vaddr & ~PAGE_MASK;
   int pages = (size+(vaddr&PAGE_MASK)+(PAGE_SIZE-1))/PAGE_SIZE;
   for(int i = 0; i < pages; i++) {
      uint32_t page_addr = base + i*PAGE_SIZE;
      int mapping = page_checkmapping(page_dir, page_addr);
      if(mapping == PAGE_USERRW) continue;
      if(!task_addr_demand_paged(task, page_addr)
      || !task_demand_map(task->process, page_addr))
         return i == 0 ? 0 : (int)(page_addr - vaddr);
   }
   return size;
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
   if(task < 0) { // kernel owned buffer
      memcpy(dest, src, size);
      return size;
   }
   
   task_state_t *task_state = &gettasks()[task];
   int maxsize = task_validate_maxsize(task_state, dest, size, 1);
   if(maxsize < 0 || (size > 0 && maxsize == 0)) return -1; // invalid buffer/size
   maxsize = task_prefault(task_state, dest, maxsize);
   if(size > 0 && maxsize == 0) return -1; // out of memory

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
   if(task < 0) { // kernel owned buffer
      memcpy(dest, src, size);
      return size;
   }

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

kobj_ref_t *process_get_kobj(process_t *process, kobj_handle_t h) {
   if(!process || h < 0) return NULL;
   int index = KOBJ_HANDLE_INDEX(h);
   if(index >= PROCESS_MAX_KOBJ) return NULL;
   kobj_ref_t *ref = process->kobj[index];
   if(!ref || !ref->inst) return NULL;
   uint32_t check = (uint32_t)h >> 16;
   if(check != KOBJ_HANDLE_CHECK(ref->uid)) {
      debug_printf("process_get_kobj: stale handle 0x%h (process %u)\n", h, process->uid);
      return NULL;
   }
   return ref;
}

kobj_ref_t *process_get_kobj_type(process_t *process, kobj_handle_t h, kobj_type_t type) { // validates type
   kobj_ref_t *ref = process_get_kobj(process, h);
   if(!ref || ref->inst->obj->type != type) return NULL;
   return ref;
}

kobj_handle_t process_acquire_kobj(process_t *process, kobj_inst_t *kobj_inst, uint32_t rights) {
   // take first free slot
   kobj_handle_t h = 0;
   while(h < PROCESS_MAX_KOBJ && process->kobj[h])
      h++;
   if(h == PROCESS_MAX_KOBJ) {
      debug_printf("Process %u hit max kobj limit\n", process->uid);
      return -1;
   }
   kobj_ref_t *ref = create_kobj_ref(kobj_inst, rights);
   if(!ref) return -1;
   process->kobj[h] = ref;
   process->kobj_count++;
   return h | (KOBJ_HANDLE_CHECK(ref->uid) << 16); // includes checkbits
}

bool process_release_kobj(process_t *process, kobj_handle_t h) {
   kobj_ref_t *ref = process_get_kobj(process, h);
   if(!ref) return false;
   if(!kobj_release(ref, process, false)) return false;
   process->kobj[KOBJ_HANDLE_INDEX(h)] = NULL;
   process->kobj_count--;
   return true;
}

void process_release_kobjs(process_t *process) {
   // when ending process

   for(int pass = 0; pass < 2; pass++) {
      bool device_pass = pass == 0;
      for(int i = 0; i < PROCESS_MAX_KOBJ; i++) {
         kobj_ref_t *ref = process->kobj[i];
         if(!ref) continue;
         bool is_device = ref->inst->obj->stop_func != NULL;
         // release devices first in pass 0
         if(is_device != device_pass) continue;
         if(!kobj_release(ref, process, true)) continue;
         process->kobj[i] = NULL;
         process->kobj_count--;
      }
   }
}

bool kobj_ref_can_copy(kobj_ref_t *ref, bool same_process, uint32_t rights) {
   uint32_t needed = same_process ? KOBJ_RIGHT_DUPLICATE : KOBJ_RIGHT_TRANSFER;
   return (ref->rights & needed) && (ref->rights & rights);
}

// duplicate handle into receive_process's kobjs
kobj_handle_t process_copy_kobj(process_t *send_process, process_t *receive_process, kobj_handle_t handle, uint32_t rights) {
   kobj_ref_t *ref = process_get_kobj(send_process, handle);
   if(!ref || !kobj_ref_can_copy(ref, send_process == receive_process, rights))
      return -1;
   uint32_t new_rights = ref->rights & rights;
   kobj_inst_t *inst = ref->inst;
   kobj_handle_t h = process_acquire_kobj(receive_process, inst, new_rights); // new handle
   if(h < 0)
      return -2;
   kobj_ref_t *new_ref = process_get_kobj(receive_process, h);
   bool success = !inst->obj->connect_func || inst->obj->connect_func(new_ref, receive_process);
   if(!success) {
      process_release_kobj(receive_process, h);
      return -1;
   }
   return h;
}

bool process_copy_stdio(process_t *send_process, process_t *receive_process) {
   for(int i = 0; i < 3; i++) {
      kobj_handle_t h = get_process_stdio(send_process, i);
      if(h < 0) continue;
      h = process_copy_kobj(send_process, receive_process, h, KOBJ_RIGHT_SAME);
      if(h < 0) return false;
      receive_process->stdio[i] = h;
   }
   return true;
}

uint32_t process_map_kobj(process_t *process, kobj_ref_t *ref, int index) {
   if(!(ref->rights & KOBJ_RIGHT_READ))
      return 0; // READ required for map
   bool rw = ref->rights & KOBJ_RIGHT_WRITE;
   kobj_inst_t *inst = ref->inst;
   kobj_mapping_t mapping;
   if(!inst->obj->map_func || !inst->obj->map_func(inst, index, &mapping))
      return 0;
   uint32_t flags = (rw ? VM_FLAG_WRITE : 0)|(mapping.no_cache ? VM_FLAG_NO_CACHE : 0);
   uint32_t offset = mapping.paddr & PAGE_MASK;
   mapping.size = page_align_up(mapping.size + offset);
   vm_region_t *region = process_alloc_vm(process, mapping.size, flags, inst);
   if(!region)
      return 0;
   int mapped = map_size(process->page_dir, mapping.paddr - offset, region->vaddr, mapping.size, 1, rw, mapping.no_cache);
   process->no_allocated += mapped;
   if(mapped != (int)mapping.size/PAGE_SIZE) {
      process_unmap_vm(process, region->vaddr);
      return 0;
   }
   return region->vaddr + offset;
}

vm_region_t *process_alloc_vm(process_t *process, uint32_t size, uint32_t flags, kobj_inst_t *inst) {
   if(size == 0)
      return NULL;

   size = page_align_up(size);

   uint32_t vmem = VM_START;
   vm_region_t *region = process->regions;
   vm_region_t *prev = NULL;
   while(region) {
      if(region->vaddr - vmem >= size) {
         // found gap at vmem
         break;
      }
      vmem = region->vaddr + region->size;
      prev = region;
      region = region->next;
   }

   uint32_t end = vmem + size;
   if(end < VM_START || end > VM_END)
      return NULL;
   
   vm_region_t *new = kmalloc(sizeof(vm_region_t));
   if(!new)
      return NULL;
   new->flags = flags;
   new->vaddr = vmem;
   new->size = size;
   new->next = region;
   new->inst = inst;
   kobj_add_ref(inst); // mapping holds a ref count even if handle is closed
   if(prev) {
      prev->next = new;
   } else {
      process->regions = new;
   }
   return new;
}

// find which vm region addr is in
vm_region_t *process_find_vm(process_t *process, uint32_t vaddr) {
   vm_region_t *region = process->regions;
   while(region) {
      if(vaddr >= region->vaddr && vaddr - region->vaddr < region->size)
         return region;
      region = region->next;
   }
   return NULL;
}

bool process_unmap_vm(process_t *process, uint32_t vaddr) {
   vm_region_t *prev = NULL;
   vm_region_t *region = process->regions;
   while(region) {
      if(vaddr >= region->vaddr && vaddr - region->vaddr < region->size) {
         // found
         process->no_allocated -= unmap_size(process->page_dir, region->vaddr, region->size);

         kobj_release_ref(region->inst);

         if(prev)
            prev->next = region->next;
         else
            process->regions = region->next;

         kfree(region);

         return true;
      }
      prev = region;
      region = region->next;
   }
   return false;
}

// free all vm (ie on process end)
void process_free_vm(process_t *process) {
   vm_region_t *region = process->regions;
   while(region) {
      kobj_release_ref(region->inst);
      vm_region_t *next = region->next;
      kfree(region);
      region = next;
   }

   process->regions = NULL;
}

uint32_t get_task_event_counter() {
   return task_event_counter;
}

kobj_handle_t get_process_stdio(process_t *process, int fd) {
   if(fd < 0 || fd > 2) return -1;
   kobj_handle_t h = process->stdio[fd];
   if(h < 0) {
      switch(fd) {
         case 0:
            h = fs_open(process, "/dev/stdin", FS_FLAG_READONLY);
            break;
         case 1:
            h = fs_open(process, "/dev/stdout", FS_FLAG_WRITEONLY);
            break;
         case 2:
            h = fs_open(process, "/dev/stderr", FS_FLAG_WRITEONLY);
            break;
      }
   }
   process->stdio[fd] = h;
   return h;
}

// respects stdio aliases
kobj_handle_t process_resolve_handle(process_t *process, kobj_handle_t h) {
   if(h >= 0 && h <= 2)
      return get_process_stdio(process, h);
   return h;
}

void tasks_tick() {
   if(!switching) return;
   get_current_task_state()->ticks++;   
}
