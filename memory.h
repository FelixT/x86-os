#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define KERNEL_START 0x18000 // loaded to here in bootloader1
#define KERNEL_END 0x38000 // KERNEL_START + 0x20000
#define STACKS_START 0x38000
#define TOS_KERNEL 0x40000 // STACKS_START + 0x2000
#define TOS_PROGRAM 0x50000 // TOS_KERNEL + 0x10000
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
void memcpy(void *dest, const void *src, int bytes);
int memcmp(const void *a, const void *b, int bytes);
void memcpy_fast(void *dest, const void *src, size_t bytes);
mem_segment_status_t *memory_get_table();


#endif