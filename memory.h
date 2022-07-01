#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define KERNEL_HEAP_SIZE 0x0100000 // bytes
#define MEM_BLOCK_SIZE 0x200 // 512 bytes

typedef struct mem_segment_status_t {
   bool allocated;
} mem_segment_status_t;

void memory_reserve(uint32_t offset, int bytes);
void free(uint32_t offset, int bytes);
void memory_init();
void *malloc(int bytes);
mem_segment_status_t *memory_get_table();


#endif