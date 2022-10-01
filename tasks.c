#include "tasks.h"

extern uint32_t tos_program;
uint32_t USR_CODE_SEG = 8*3;
uint32_t USR_DATA_SEG = 8*4;

task_state_t tasks[TOTAL_TASKS];
int current_task = 0;
bool switching = false; // preemptive multitasking

void create_task_entry(int index, uint32_t entry, uint32_t size, bool privileged) {
   tasks[index].enabled = false;
   tasks[index].stack_top = (uint32_t)(&tos_program - (TASK_STACK_SIZE * index));
   tasks[index].prog_entry = entry;
   tasks[index].prog_size = size;
   tasks[index].privileged = privileged;
   
   tasks[index].registers.esp = tasks[index].stack_top;
   tasks[index].registers.eax = USR_DATA_SEG | 3;
   tasks[index].registers.eip = entry;
}

void launch_task(int index, registers_t *regs, bool focus) {
   current_task = index;

   tasks[current_task].enabled = true;
   tasks[current_task].registers.ds = USR_DATA_SEG | 3;
   tasks[current_task].registers.cs = USR_CODE_SEG | 3; // user code segment

   tasks[current_task].registers.esp = regs->esp; // ignored
   tasks[current_task].registers.eflags = regs->eflags;
   tasks[current_task].registers.useresp = tasks[current_task].stack_top; // stack_top
   tasks[current_task].registers.ss = USR_DATA_SEG | 3;

   int tmpwindow = gui_get_selected_window();
   tasks[current_task].window = gui_window_add();
   if(!focus) gui_set_selected_window(tmpwindow);
   
   *regs = tasks[current_task].registers;
}

void end_task(int index, registers_t *regs) {
   if(index < 0 || index >= TOTAL_TASKS) return;
   //if(!tasks[index].enabled) return;

   gui_window_writestr("Ending task ", 0, 0);
   gui_window_writenum(index, 0, 0);
   gui_window_writestr("\n", 0, 0);

   tasks[index].enabled = false;
   tasks[index].privileged = false;

   // free task memory
   if(tasks[index].prog_size != 0)
      free(tasks[index].prog_entry, tasks[index].prog_size);

   if(index == get_current_task())
      switch_task(regs);
}

void tasks_init(registers_t *regs) {
   // enable preemptive multitasking

   for(int i = 0; i < TOTAL_TASKS; i++)
      tasks[i].enabled = false;

   // launch idle process
   fat_dir_t *entry = fat_parse_path("/sys/progidle.bin");
   if(entry == NULL) {
      gui_writestr("Not found\n", 0);
      return;
   }
   uint8_t *prog = fat_read_file(entry->firstClusterNo, entry->fileSize);
   uint32_t idleentry = (uint32_t)prog;
   create_task_entry(0, idleentry, entry->fileSize, false);
   launch_task(0, regs, false);

   switching = true;
}

bool task_exists() {
   for(int i = 0; i < TOTAL_TASKS; i++) {
      if(tasks[i].enabled) return true;
   }
   return false;
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

      // restore registers
      *regs = tasks[current_task].registers;
   } else {
      tasks_init(regs);
   }
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
      if(tasks[i].window == windowIndex)
         return i;
   }
   return -1;
}