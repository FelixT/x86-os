#include <stdbool.h>

extern uint32_t heap_kernel;

#define KERNEL_HEAP_SIZE 0x0100000 // bytes
#define MEM_BLOCK_SIZE 0x200 // 512 bytes

typedef struct mem_segment_status_t {
   bool allocated;
} mem_segment_status_t;

mem_segment_status_t memory_status[KERNEL_HEAP_SIZE/MEM_BLOCK_SIZE];

void memory_init() {
   for(int i = 0; i < KERNEL_HEAP_SIZE/MEM_BLOCK_SIZE; i++) {
      memory_status[i].allocated = false;
   }
}

void *malloc(int bytes) {
   // find continuous block of free memory of size bytes
   int noBlocks = (bytes+(MEM_BLOCK_SIZE-1))/MEM_BLOCK_SIZE;  // rounding up

   int blocksRemaining = noBlocks;
   int index = 0;
   int blockStart = 0;

   while(blocksRemaining > 0) {
      if(index >= KERNEL_HEAP_SIZE/MEM_BLOCK_SIZE) {
         return NULL; // couldn't find a block large enough
      }

      if(!memory_status[index].allocated) {
         // start of potential block
         if(blocksRemaining == noBlocks) {
            blockStart = index;
         }
         blocksRemaining--;
      } else {
         // block has memory already allocated within it, disregard
         blocksRemaining = noBlocks;
      }

      index++;
   }

   // we've found a block, update memory_status
   for(int i = blockStart; i < blockStart+noBlocks; i++)
      memory_status[i].allocated = true;

   return (void*)((int)(&heap_kernel) + (int)(blockStart*MEM_BLOCK_SIZE));
}

// TODO: memset, memcpy, memmove and memcmp 