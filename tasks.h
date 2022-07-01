#ifndef TASKS_H
#define TASKS_H

// general registers in order they are pushed onto stack
// https://faydoc.tripod.com/cpu/pusha.htm
typedef struct registers_t {
   uint32_t ds;
   uint32_t edi; // saved with pusha
   uint32_t esi;
   uint32_t ebp;
   uint32_t esp; // ignored by popa
   uint32_t ebx;
   uint32_t edx;
   uint32_t ecx;
   uint32_t eax;
   uint32_t err_code; // or dummy
   uint32_t eip, cs, eflags, useresp, ss; // automatically pushed upon interrupt
} __attribute__((packed)) registers_t;

typedef struct task_state_t {
   bool enabled;
   uint32_t stack_top; // address
   registers_t registers;
} task_state_t;

#define TOTAL_STACK_SIZE 0x0010000
#define TASK_STACK_SIZE 0x0001000
#define TOTAL_TASKS 4

void create_task_entry(int index, uint32_t entry);

void launch_task(int index, registers_t *regs);

void tasks_init(registers_t *regs);

void switch_task(registers_t *regs);

task_state_t *gettasks();

#endif