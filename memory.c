// https://wiki.osdev.org/Memory_Map_(x86)

#include "memory.h"
#include "gui.h"

#define MEM_DEBUG 0

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

extern void debug_writestr(char* str);
extern void debug_writeuint(uint32_t num);

void free(uint32_t offset, int bytes) {
   if(offset == 0 || bytes == 0) return;

   int blockStart = ((int)offset-(int)HEAP_KERNEL)/MEM_BLOCK_SIZE;
   int noBlocks = (bytes+(MEM_BLOCK_SIZE-1))/MEM_BLOCK_SIZE;


   for(int i = 0; i < noBlocks; i++) {
      int block = blockStart+i;
      if(block >= 0 && block < KERNEL_HEAP_SIZE/MEM_BLOCK_SIZE) {
         memory_status[block].allocated = false;
      }
   }
   
   #ifdef MEM_DEBUG
   // fill free memory with 'FREE' in ascii
   char freeASCII[6] = "FREE ";
   for(int x = 0; x < noBlocks*MEM_BLOCK_SIZE; x++) {
      char *byte = (char*) ((HEAP_KERNEL) + (int)blockStart*MEM_BLOCK_SIZE) + x;
      *byte = freeASCII[x%6];
   }
   #endif
}

void memory_init() {
   for(int i = 0; i < KERNEL_HEAP_SIZE/MEM_BLOCK_SIZE; i++) {
      memory_status[i].allocated = false;
   }
   #ifdef MEM_DEBUG
   // fill free memory with 'FREE' in ascii
   char freeASCII[6] = "FREE ";
   for(int i = 0; i < KERNEL_HEAP_SIZE; i++) {
      char *byte = (char*) ((HEAP_KERNEL) + i);
      *byte = freeASCII[i%6];
   }
  #endif
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
   for(int i = blockStart; i < blockStart+noBlocks; i++) {
      if(memory_status[i].allocated) debug_writestr("Already allocated\n");
      memory_status[i].allocated = true;
   }

   #ifdef MEM_DEBUG
   // fill allocated memory with 'ALLC' in ascii
   char allcASCII[6] = "ALLC ";
   for(int i = 0; i < noBlocks*MEM_BLOCK_SIZE; i++) {
      char *byte = (char*) ((HEAP_KERNEL) + (int)((blockStart)*MEM_BLOCK_SIZE)) + i;
      *byte = allcASCII[i%6];
   }
   #endif

   int addr = (int)(HEAP_KERNEL) + (int)(blockStart*MEM_BLOCK_SIZE);

   return (void*)addr;
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

void memset(void *dest, uint8_t ch, int count) {
   // faster memset using fancy assembly
   // Use stosb for small blocks (under 16 bytes)
   if(count < 16) {
      asm volatile (
         "rep stosb"
         :
         : "D" (dest), "a" (ch), "c" (count)
         : "memory", "cc"
      );
      return;
   }
    
   // For larger blocks, use stosd (4 bytes at a time)
   uint32_t value = ch;
   value |= value << 8;
   value |= value << 16;  // Replicate the byte across all 4 bytes
   
   // Handle unaligned prefix bytes
   int pre = (4 - ((uintptr_t)dest & 3)) & 3;
   if(pre > 0) {
      if(pre > count) pre = count;
      asm volatile (
         "rep stosb"
         :
         : "D" (dest), "a" (ch), "c" (pre)
         : "memory", "cc"
      );
      dest = (char*)dest + pre;
      count -= pre;
   }
   
   // Handle the bulk with stosd (4 bytes at a time)
   size_t dwords = count / 4;
   if(dwords > 0) {
      asm volatile (
         "rep stosl"
         :
         : "D" (dest), "a" (value), "c" (dwords)
         : "memory", "cc"
      );
      dest = (char*)dest + (dwords * 4);
      count &= 3;
   }
   
   // Handle trailing bytes
   if(count > 0) {
      asm volatile (
         "rep stosb" :
         : "D" (dest), "a" (ch), "c" (count)
         : "memory", "cc"
      );
   }

}

void memcpy(void *dest, const void *src, int bytes) {
   for(int i = 0; i < bytes; i++)
      ((uint8_t*)dest)[i] = ((uint8_t*)src)[i];
}

int memcmp(const void *a, const void *b, int bytes) {
   const uint8_t *pa = (const uint8_t*)a;
   const uint8_t *pb = (const uint8_t*)b;
   for(int i = 0; i < bytes; i++) {
      if(pa[i] != pb[i])
           return pa[i] - pb[i];
   }
   return 0; 
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

/*void memcpy_fast_old(void *dest, const void *src, size_t bytes) {
   uint8_t *d = (uint8_t*)dest;
   const uint8_t *s = (const uint8_t*)src;

   // align dest to 4 bytes if necessary
   while(((uintptr_t)d & 3) && bytes > 0) {
      *d++ = *s++;
      bytes--;
   }

   // if src is also 4 byte aligned copy 4 bytes at a time
   if(((uintptr_t)s & 3) == 0) {
      uint32_t *d32 = (uint32_t*)d;
      const uint32_t *s32 = (const uint32_t*)s;

      while(bytes >= 4) {
         *d32++ = *s32++;
         bytes -= 4;
      }

      d = (uint8_t*)d32;
      s = (const uint8_t*)s32;
   }

   // copy remaining bytes
   while(bytes--)
      *d++ = *s++;
}*/

// TODO: memmove 