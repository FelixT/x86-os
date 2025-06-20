#ifndef TASKS_H
#define TASKS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "gui.h"
#include "fat.h"
#include "memory.h"
#include "gdt.h"
#include "interrupts.h"
#include "registers_t.h"
#include "elf.h"
#include "paging.h"

typedef struct task_state_t {
   bool enabled;
   bool paused; // task won't be scheduled
   uint32_t stack_top; // address
   uint32_t prog_start; // physical addr of start of program
   uint32_t prog_entry; // addr of program entry point, may be different from prog_start
   uint32_t prog_size;
   registers_t registers;
   bool privileged; // 0 = user, 1 = kernel
   int window;
   uint32_t vmem_start; // virtual address where program is loaded
   uint32_t vmem_end;
   registers_t routine_return_regs;
   int routine_return_window; // switch to this window after routine
   uint32_t *routine_args;
   int routine_argc;
   bool in_routine;
   char routine_name[32];
   page_dir_entry_t *page_dir;
   uint32_t *allocated_pages[128]; // pointers as returned by malloc
   uint16_t no_allocated;
   bool in_syscall;
   uint16_t syscall_no;
   char working_dir[256]; // current working directory
} task_state_t;

#define TOTAL_STACK_SIZE 0x0010000
#define TASK_STACK_SIZE 0x0001000
#define TOTAL_TASKS 8

void create_task_entry(int index, uint32_t entry, uint32_t size, bool privileged);
void launch_task(int index, registers_t *regs, bool focus);
void end_task(int index, registers_t *regs);
void tasks_alloc();
void tasks_init(registers_t *regs);
void switch_task(registers_t *regs);
bool switch_to_task(int index, registers_t *regs);
void tasks_launch_binary(registers_t *regs, char *path);
void tasks_launch_elf(registers_t *regs, char *path, int argc, char **args);

void pause_task(int index, registers_t *regs);

task_state_t *gettasks();

int get_current_task_window();
int get_task_window(int task);
int get_current_task();
int get_task_from_window(int windowIndex);
int get_free_task_index();

void task_call_subroutine(registers_t *regs, char *name, uint32_t addr, uint32_t *args, int argc);
void task_subroutine_end(registers_t *regs);
void tss_init();

#endif