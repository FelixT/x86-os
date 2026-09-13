#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// assuming a total memory size of >~64mb
// kernel is loaded into KERNEL_START

// physical memory layout
#define KERNEL_SIZE  0x40000 // kernel binary size (~ the real size of 258k)

#define KERNEL_START 0x1000000 // loaded to here in bootloader1
#define KERNEL_END   0x1040000 // KERNEL_START + KERNEL_SIZE

// kstack used for kernel page dir (before any tasks are launched, each task has its own kstack located in heap)
#define KSTACK_START 0x160000
#define TOS_KERNEL   0x164000 // KSTACK_START + KSTACK_SIZE
#define KSTACK_SIZE  0x04000

#define KSTACK_DF_START 0x164000 // separate kstack for running double fault exception handler
#define KSTACK_DF_TOS 0x168000

#define HEAP_KERNEL     0x1040000 // unified physical heap for user & kernel
#define HEAP_KERNEL_END 0x3040000 // HEAP_KERNEL + 0x2000000

#define KERNEL_HEAP_SIZE 0x2000000 // bytes
#define MEM_BLOCK_SIZE   0x1000 // 4096 bytes (page size) for now (previously 0x200/512 bytes)

// virtual memory layout
// todo: kernel mapped to highest half of memory (0xC0000000-0xFFFFFFFF)
// currently, loaded & identity mapped to known location

// physical -> virtual offset is 0 for now (V_KERNEL_START-KERNEL_START)
#define V_KERNEL_START 0x1000000
#define V_KERNEL_END   0x1040000 // V_KERNEL_START + 0x20000 (kernel size 0x20000)

#define V_SHARED_START 0xA0000000
#define V_SHARED_END   0xB0000000

#define V_MMIO_START 0xB0000000 // start for each process
#define V_MMIO_END   0xC0000000

#define V_STACKS_START 0xC0000000 // program stack for each mapped to known location (V_STACKS_START+i*TASK_STACK_SIZE)

typedef struct mem_segment_status_t {
   bool allocated;
} mem_segment_status_t;

void memory_reserve(uint32_t offset, int bytes);
void free(uint32_t offset, int bytes);
void memory_init();
void *malloc(int bytes);
void *resize(uint32_t offset, int oldsize, int newsize);
void memcpy_fast(void *dest, const void *src, size_t bytes);
mem_segment_status_t *memory_get_table();

#endif