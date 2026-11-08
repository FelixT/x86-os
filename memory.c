// https://wiki.osdev.org/Memory_Map_(x86)

#include "memory.h"
#include "gui.h"

// page/'block' allocator

#define BLOCK_COUNT (KERNEL_HEAP_SIZE/PAGE_SIZE)

int head = 0; // block num of first free entry

uint32_t block_bitmap[BLOCK_COUNT/32];

static inline bool valid_block(int block) {
   return block >= 0 && block < KERNEL_HEAP_SIZE/PAGE_SIZE;
}

static bool set_blocks(int blockStart, int noBlocks, bool value) {
   if(noBlocks <= 0 || !valid_block(blockStart) || !valid_block(blockStart+noBlocks-1))
      return false;
   int start = blockStart/32;
   int end = (blockStart+noBlocks-1)/32;
   for(int i = start; i <= end; i++) {
      int offset_bytes = (i == start) ? blockStart%32 : 0; // lo
      int skip_bytes = (i == end) ? 31-(blockStart+noBlocks-1)%32 : 0; // hi
      uint32_t mask = (0xFFFFFFFF>>skip_bytes)&(0xFFFFFFFF<<offset_bytes);
      if(value)
         block_bitmap[i] |= mask;
      else
         block_bitmap[i] &= ~mask;
   }
   return true;
}

void memory_reserve(uint32_t offset, int bytes) {
   int noBlocks = (bytes+(PAGE_SIZE-1))/PAGE_SIZE;  // rounding up
   int blockStart = ((int)offset-(int)HEAP_KERNEL)/PAGE_SIZE;

   set_blocks(blockStart, noBlocks, true);
}

void free(uint32_t offset, int bytes) {
   if(offset == 0 || bytes == 0) return;

   int blockStart = ((int)offset-(int)HEAP_KERNEL)/PAGE_SIZE;
   int noBlocks = (bytes+(PAGE_SIZE-1))/PAGE_SIZE;

   if(set_blocks(blockStart, noBlocks, false) && blockStart < head)
      head = blockStart;
}

void memory_init() {
   // set all to unallocated
   memset(block_bitmap, 0, sizeof(uint32_t)*(BLOCK_COUNT/32));
}

static inline bool block_used(int n) { return block_bitmap[n/32] & (1u << (n%32)); }

static int next_free(int n) { // first free block at or after n
   while(n < BLOCK_COUNT) {
      uint32_t free_bits = ~block_bitmap[n/32] & (0xFFFFFFFFu << (n%32));
      if(free_bits) return (n & ~31) + __builtin_ctz(free_bits);
      n = (n & ~31) + 32;
   }
   return BLOCK_COUNT;
}

void *malloc(int bytes) {
   if(bytes <= 0) return NULL;
   int noBlocks = (bytes + PAGE_SIZE-1)/PAGE_SIZE;
   int blockStart = 0;
   int blockLength = 0;
   for(int n = head; n < BLOCK_COUNT; n++) {
      if(n%32 == 0 && block_bitmap[n/32] == 0xFFFFFFFF) { n += 31; blockLength = 0; continue; }
      if(block_used(n)) { blockLength = 0; continue; }
      if(blockLength++ == 0) blockStart = n;
      if(blockLength == noBlocks) {
         set_blocks(blockStart, noBlocks, true);
         if(blockStart == head) head = next_free(blockStart + noBlocks);
         return (void*)(HEAP_KERNEL + blockStart*PAGE_SIZE);
      }
   }
   return NULL;
}

void *resize(uint32_t offset, int oldsize, int newsize) {
   uint8_t *newaddr = malloc(newsize);
   if(newaddr == NULL)
      return NULL;
   uint8_t *oldaddr = (uint8_t *)offset;
   memcpy(newaddr, oldaddr, oldsize < newsize ? oldsize : newsize);
   free(offset, oldsize);
   return newaddr;
}

uint32_t *memory_get_bitmap() {
   return block_bitmap;
}

int allocated_blocks() {
   int total = 0;
   for(int i = 0; i < BLOCK_COUNT/32; i++) {
      uint32_t x = block_bitmap[i];
      while(x) {
         total += (x & 1);
         x >>= 1;
      }
   }
   return total;
}

void memcpy_fast(void *dest, const void *src, size_t bytes) {
   // Use rep movsd for 4-byte blocks, then rep movsb for remaining bytes
   unsigned int dword_count = bytes / 4;
   unsigned int byte_remainder = bytes % 4;

   asm volatile (
      "cld\n\t"
      "rep movsl\n\t"
      "mov %3, %%ecx\n\t"
      "rep movsb"
      : "+D" (dest), "+S" (src), "+c" (dword_count)
      : "r" (byte_remainder)
      : "memory"
   );

}
