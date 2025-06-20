#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// assuming a total memory size of 128mb
// kernel is loaded into 100mb

// physical memory layout
#define KERNEL_SIZE 0x20000 // kernel binary size (~ the real size of 128k)

#define KERNEL_START 0x06400000 // loaded to here in bootloader1
#define KERNEL_END 0x06420000 // KERNEL_START + KERNEL_SIZE

#define STACKS_START 0x40000
#define TOS_PROGRAM 0x50000 // TOS_KERNEL + 0x10000
#define HEAP_KERNEL 0x100000 // unified physical heap for user & kernel
#define HEAP_KERNEL_END 0x2100000 // HEAP_KERNEL + 0x2000000

#define KSTACK_START 0x06420000
#define TOS_KERNEL 0x06422000 // V_STACKS_START + 0x2000 (k stack size 0x2000)

#define KERNEL_HEAP_SIZE 0x2000000 // bytes
#define MEM_BLOCK_SIZE 0x1000 // 4096 bytes (page size) for now (previously 0x200/512 bytes)

// virtual memory layout
// kernel mapped to highest half of memory (0xC0000000-0xFFFFFFFF)

// physical -> virtual offset is 0xbffe8000 (V_KERNEL_START-KERNEL_START)
#define V_KERNEL_START 0x06400000
#define V_KERNEL_END 0x06420000 // V_KERNEL_START + 0x20000 (kernel size 0x20000)
#define V_KSTACK_START 0x06420000 // V_KERNEL_END -> V_KERNEL_START + 0x20000 (kernel size 0x20000)
#define V_TOS_KERNEL 0x06422000 // V_STACKS_START + 0x2000 (k stack size 0x2000)
#define V_HEAP_KERNEL 0x06500000 // kernel specific heap (? - should map this dynamically)
#define V_HEAP_KERNEL_END 0x08000000 // V_HEAP_KERNEL + 0x2000000 (heap size 0x2000000) - this exceeds 128mb but should be sound

typedef struct mem_segment_status_t {
   bool allocated;
} mem_segment_status_t;

void memory_reserve(uint32_t offset, int bytes);
void free(uint32_t offset, int bytes);
void memory_init();
void *malloc(int bytes);
void *resize(uint32_t offset, int oldsize, int newsize);
void memcpy(void *dest, const void *src, int bytes);
int memcmp(const void *a, const void *b, int bytes);
void memcpy_fast(void *dest, const void *src, size_t bytes);
mem_segment_status_t *memory_get_table();


#endif