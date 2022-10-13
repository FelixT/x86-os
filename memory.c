// https://wiki.osdev.org/Memory_Map_(x86)

#include "memory.h"
#include "gui.h"

mem_segment_status_t memory_status[KERNEL_HEAP_SIZE/MEM_BLOCK_SIZE];

void memory_reserve(uint32_t offset, int bytes) {
   int noBlocks = (bytes+(MEM_BLOCK_SIZE-1))/MEM_BLOCK_SIZE;  // rounding up
   int blockStart = ((int)offset-(int)HEAP_KERNEL)/MEM_BLOCK_SIZE;

   for(int i = 0; i < noBlocks; i++) {
      if(blockStart+i >= 0 && blockStart+i < KERNEL_HEAP_SIZE/MEM_BLOCK_SIZE) {
         memory_status[blockStart+i].allocated = true;
      }
   }
}

void free(uint32_t offset, int bytes) {
   if(offset == 0 || bytes == 0) return;

   int blockStart = ((int)offset-(int)HEAP_KERNEL)/MEM_BLOCK_SIZE;
   int noBlocks = (bytes+(MEM_BLOCK_SIZE-1))/MEM_BLOCK_SIZE;

   /*gui_window_writestr("Freeing ", 0, 0);
   gui_window_writenum(blockStart, 0, 0);
   gui_window_writestr(" <", 0, 0);
   gui_window_writenum(noBlocks, 0, 0);
   gui_window_writestr(">\n", 0, 0);*/

   char freeASCII[6] = "FREE ";

   for(int i = 0; i < noBlocks; i++) {
      int block = blockStart+i;
      if(block >= 0 && block < KERNEL_HEAP_SIZE/MEM_BLOCK_SIZE) {
         //gui_window_writenum(block, 0, 0);
         //gui_window_writestr(" ", 0, 0);
         memory_status[block].allocated = false;
      }
   }

   for(int x = 0; x < noBlocks*MEM_BLOCK_SIZE; x++) {
      char *byte = (char*) ((HEAP_KERNEL) + (int)blockStart*MEM_BLOCK_SIZE) + x;
      *byte = freeASCII[x%6];
   }
}

void memory_init() {
   for(int i = 0; i < KERNEL_HEAP_SIZE/MEM_BLOCK_SIZE; i++) {
      memory_status[i].allocated = false;
   }
   // fill free memory with 'FREE' in ascii
   char freeASCII[6] = "FREE ";
   for(int i = 0; i < KERNEL_HEAP_SIZE; i++) {
      char *byte = (char*) ((HEAP_KERNEL) + i);
      *byte = freeASCII[i%6];
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

   // fill allocated memory with 'ALLC' in ascii
   char allcASCII[6] = "ALLC ";
   for(int i = 0; i < noBlocks*MEM_BLOCK_SIZE; i++) {
      char *byte = (char*) ((HEAP_KERNEL) + (int)((blockStart)*MEM_BLOCK_SIZE)) + i;
      *byte = allcASCII[i%6];
   }

   return (void*)((int)(HEAP_KERNEL) + (int)(blockStart*MEM_BLOCK_SIZE));
}

void *resize(uint32_t offset, int oldsize, int newsize) {
   // TODO

   // if current memory location block can be extended, extend
   // otherwise malloc the new size, copy from old to new then free old
   uint8_t *newaddr = malloc(newsize);
   if(newaddr == NULL)
      return NULL;
   uint8_t *oldaddr = (uint8_t *)offset;
   for(int i = 0; i < oldsize; i++)
      newaddr[i] = oldaddr[i];
   free(offset, oldsize);
   return newaddr;
}

mem_segment_status_t *memory_get_table() {
   return &memory_status[0];
}

// TODO: memset, memcpy, memmove and memcmp 