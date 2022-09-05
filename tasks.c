#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "tasks.h"
#include "gui.h"

extern uint32_t tos_program;
uint32_t USR_CODE_SEG = 8*3;
uint32_t USR_DATA_SEG = 8*4;

task_state_t tasks[TOTAL_TASKS];
int current_task = 0;
bool switching = false; // preemptive multitasking

void create_task_entry(int index, uint32_t entry) {
   tasks[index].enabled = true;
   tasks[index].stack_top = (uint32_t)(&tos_program - (TASK_STACK_SIZE * index));
   
   tasks[index].registers.esp = tasks[index].stack_top;
   tasks[index].registers.eax = USR_DATA_SEG | 3;
   tasks[index].registers.eip = entry;
}

void launch_task(int index, registers_t *regs) {
   current_task = index;

   tasks[current_task].registers.ds = USR_DATA_SEG | 3;
   tasks[current_task].registers.cs = USR_CODE_SEG | 3; // user code segment

   tasks[current_task].registers.esp = regs->esp; // ignored
   tasks[current_task].registers.eflags = regs->eflags;
   tasks[current_task].registers.useresp = tasks[current_task].stack_top; // stack_top
   tasks[current_task].registers.ss = USR_DATA_SEG | 3;

   *regs = tasks[current_task].registers;
}

void tasks_init(registers_t *regs) {
   for(int i = 0; i < TOTAL_TASKS; i++) {
      tasks[i].enabled = false;
   }
   uint32_t idleentry = 48000+0x7c00;
   gui_window_writenum(idleentry, 0, 0);
   create_task_entry(0, idleentry);
   launch_task(0, regs);

   switching = true;
}

void switch_task(registers_t *regs) {
   if(!switching)
      return;
   
   // save registers
   tasks[current_task].registers = *regs;


   // find next enabled task
   do {
      current_task++;
      current_task%=TOTAL_TASKS;
   } while(!tasks[current_task].enabled);

   // restore registers
   *regs = tasks[current_task].registers;
}

task_state_t *gettasks() {
   return &tasks[0];
}