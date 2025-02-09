#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define KERNEL_START 0x07000  // kernel loads at 0x7e00 but we map from here for 4KiB align
#define KERNEL_END   0x18000 // kernel really ends at (0x7e00+0x10000=0x17e00)
#define STACKS_START 0x18000
#define TOS_KERNEL 0x19000 // STACKS_START + 0x1000
#define TOS_PROGRAM 0x29000 // TOS_KERNEL + 0x10000
#define HEAP_KERNEL 0x100000
#define HEAP_KERNEL_END 0x2100000 // HEAP_KERNEL + 0x2000000

#define KERNEL_HEAP_SIZE 0x2000000 // bytes
#define MEM_BLOCK_SIZE 0x1000 // 4096 bytes (page size) for now (previously 0x200/512 bytes)

typedef struct mem_segment_status_t {
   bool allocated;
} mem_segment_status_t;

void memory_reserve(uint32_t offset, int bytes);
void free(uint32_t offset, int bytes);
void memory_init();
void *malloc(int bytes);
void *resize(uint32_t offset, int oldsize, int newsize);
void memset(void *dest, uint8_t ch, int bytes);
void memcpy(void *dest, void *src, int bytes);
mem_segment_status_t *memory_get_table();


#endif