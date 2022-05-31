#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// general registers in order they are pushed onto stack
// https://faydoc.tripod.com/cpu/pusha.htm
typedef struct registers_t {
   uint32_t edi; // saved with pusha
   uint32_t esi;
   uint32_t ebp;
   uint32_t esp;
   uint32_t ebx;
   uint32_t edx;
   uint32_t ecx;
   uint32_t eax;
   uint32_t eip, cs, eflags, useresp, ss; // automatically pushed upon interrupt
} registers_t;

// size = 13 * 32

typedef struct task_state_t {
   bool enabled;
   uint32_t stack_top; // address
   registers_t registers;
} task_state_t;

extern uint32_t tos_program;

#define TOTAL_STACK_SIZE 0x0010000
#define TASK_STACK_SIZE 0x0001000
#define TOTAL_TASKS 3

task_state_t tasks[TOTAL_TASKS];
int current_task = 0;
bool switching = false; // preemptive multitasking

void create_task_entry(int index, uint32_t entry) {
   tasks[index].enabled = true;
   tasks[index].stack_top = (uint32_t)(&tos_program - (TASK_STACK_SIZE * index));
   tasks[index].registers.esp = tasks[index].stack_top;
   tasks[index].registers.eax = 0x10;
   tasks[index].registers.eip = entry;
}

void launch_task(int index, registers_t *regs) {
   current_task = index;
   
   // set flags to current used values (temporary fix)
   tasks[current_task].registers.esp = regs->esp;
   tasks[current_task].registers.cs = regs->cs;
   tasks[current_task].registers.eflags = regs->eflags;
   tasks[current_task].registers.useresp = regs->useresp;
   tasks[current_task].registers.ss = regs->ss;

   *regs = tasks[current_task].registers;
}

void tasks_init(registers_t *regs) {
   for(int i = 0; i < TOTAL_TASKS; i++) {
      tasks[i].enabled = false;
   }
   uint32_t idleentry = 25000+0x7c00;
   create_task_entry(0, idleentry);
   switching = true;
   launch_task(0, regs);
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