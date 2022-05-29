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
   bool active;
   uint32_t stack_top; // address
   registers_t registers;
} task_state_t;

extern uint32_t tos_program;

#define TOTAL_STACK_SIZE 0x0010000
#define TASK_STACK_SIZE 0x0001000
#define TOTAL_TASKS 2

task_state_t tasks[TOTAL_TASKS];
int current_task = 0;

void create_task_entry(int index, uint32_t entry) {
   tasks[index].active = false;
   tasks[index].stack_top = (uint32_t)(&tos_program - (TASK_STACK_SIZE * index));
   tasks[index].registers.esp = tasks[index].stack_top;
   tasks[index].registers.eax = 0x10;
   tasks[index].registers.eip = entry;
}