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
#include "fs.h"

#define TOTAL_STACK_SIZE 0x0010000
#define TASK_STACK_SIZE 0x0001000

#define TOTAL_TASKS 8
#define MAX_TASK_THREADS 4

#define USR_CODE_SEG (8*3)
#define USR_DATA_SEG (8*4)

typedef struct task_state_t task_state_t;

typedef struct {
   char *name;
   uint32_t addr; // subroutine addr
   uint32_t *args;
   int argc;
   int task;
   int window;
} task_event_t;

#define EVENT_QUEUE_SIZE 64

// process struct - shared between threads
typedef struct process_t {
   uint32_t prog_start; // physical addr of start of program
   uint32_t prog_entry; // addr of program entry point, may be different from prog_start
   uint32_t prog_size;
   bool privileged;
   int window; // the task's main window. additional windows are children of this
   uint32_t vmem_start; // virtual address where program is loaded
   uint32_t vmem_end;
   page_dir_entry_t *page_dir;
   uint32_t *allocated_pages[128]; // pointers as returned by malloc
   int no_allocated;
   char working_dir[256]; // current working directory
   char exe_path[256]; // location of executable
   uint32_t heap_start; // heap/end of ds (vmem location)
   uint32_t heap_end;
   fs_file_t *file_descriptors[64];
   int fd_count;
   task_event_t *event_queue[EVENT_QUEUE_SIZE];
   int event_queue_size;

   task_state_t *threads[MAX_TASK_THREADS];
   int no_threads;
} process_t;

typedef struct task_state_t {
   bool enabled;
   bool paused; // thread won't be scheduled
   int task_id;
   uint32_t stack_top; // address
   registers_t registers;
   registers_t routine_return_regs;
   int routine_return_window; // switch to this window after routine, potentially unneeded
   uint32_t *routine_args;
   int routine_argc;
   bool in_routine;
   char routine_name[32];
   bool in_syscall;
   uint16_t syscall_no;

   process_t *process; // parent process
} task_state_t;

void create_task_entry(int index, uint32_t entry, uint32_t size, bool privileged, process_t *process);
void launch_task(int index, registers_t *regs, bool focus);
void end_task(int index, registers_t *regs);
void tasks_alloc();
void tasks_init(registers_t *regs);
void switch_task(registers_t *regs);
bool switch_to_task(int index, registers_t *regs);
void tasks_launch_binary(registers_t *regs, char *path);
bool tasks_launch_elf(registers_t *regs, char *path, int argc, char **args, bool focus);

void pause_task(int index, registers_t *regs);

task_state_t *gettasks();

int get_current_task_window();
int get_task_window(int task);
int get_current_task();
task_state_t *get_current_task_state();
page_dir_entry_t *get_current_task_pagedir();
int get_task_from_window(int windowIndex);
int get_free_task_index();

void task_call_subroutine(registers_t *regs, char *name, uint32_t addr, uint32_t *args, int argc);
void task_subroutine_end(registers_t *regs);
void tss_init();

void task_write_to_window(int task, char *out, bool children);
void task_reset_windows(int task);

#endif