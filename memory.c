// https://wiki.osdev.org/Memory_Map_(x86)

#include "memory.h"
#include "gui.h"
#include "kalloc.h"

// page/'block' allocator

#define BLOCK_COUNT (KERNEL_HEAP_SIZE/PAGE_SIZE)

int head = 0; // block num of first free entry

uint32_t block_bitmap[BLOCK_COUNT/32];
uint8_t mem_status[BLOCK_COUNT]; // mem_type_t

kcache_t mem16_cache; // 16 bytes
kcache_t mem32_cache;
kcache_t mem64_cache;
kcache_t mem128_cache;
kcache_t mem256_cache;
kcache_t mem512_cache;

void memory_init() {
   // set all to unallocated
   memset(block_bitmap, 0, sizeof(uint32_t)*(BLOCK_COUNT/32));
   memset(mem_status, MEM_FREE, BLOCK_COUNT);
   // setup caches
   kalloc_init(&mem16_cache, 4, 16);
   kalloc_init(&mem32_cache, 4, 32);
   kalloc_init(&mem64_cache, 4, 64);
   kalloc_init(&mem128_cache, 4, 128);
   kalloc_init(&mem256_cache, 4, 256);
   kalloc_init(&mem512_cache, 4, 512);
}

static inline bool valid_block(int block) {
   return block >= 0 && block < KERNEL_HEAP_SIZE/PAGE_SIZE;
}

static bool set_blocks(int blockStart, int noBlocks, bool value, mem_type_t type) {
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
   for(int i = blockStart; i < blockStart + noBlocks; i++)
      mem_status[i] = (type==MEM_HEAP&&i!=blockStart) ? (uint8_t)MEM_HEAPCONT : (uint8_t)type;
   return true;
}

void memory_reserve(uint32_t offset, int bytes) {
   int noBlocks = (bytes+(PAGE_SIZE-1))/PAGE_SIZE;  // rounding up
   int blockStart = ((int)offset-(int)HEAP_KERNEL)/PAGE_SIZE;

   set_blocks(blockStart, noBlocks, true, MEM_RESERVE);
}

extern void debug_printf(char *format, ...);

void kfree(void *mem) {
   if(!mem) return;

   int blockStart = ((int)mem-(int)HEAP_KERNEL)/PAGE_SIZE;
   if(!valid_block(blockStart)) return;
   int noBlocks = 0;

   switch(mem_status[blockStart]) {
      case MEM_FREE:
      case MEM_RESERVE:
         debug_printf("Double free detected at 0x%h\n", mem);
         kernel_panic();
         break;
      case MEM_SLAB:
         if(!kslab_is_allocated(mem)) {
            debug_printf("Bad/double slab free at 0x%h\n", mem);
            kernel_panic();
         }
         if(kdealloc(mem))
            noBlocks = 1;
         break;
      case MEM_HEAPCONT:
         debug_printf("Misaligned free at 0x%h\n", mem);
         kernel_panic();
         break;
      case MEM_HEAP:
         noBlocks++;
         while(blockStart+noBlocks < BLOCK_COUNT) {
            if(mem_status[blockStart+noBlocks] != MEM_HEAPCONT)
               break;
            noBlocks++;
         }
         break;
   }

   if(noBlocks > 0 && set_blocks(blockStart, noBlocks, false, MEM_FREE) && blockStart < head)
      head = blockStart;
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

void *kmalloc_type(int bytes, mem_type_t type) {
   if(bytes <= 0) return NULL;
   int noBlocks = (bytes + PAGE_SIZE-1)/PAGE_SIZE;
   int blockStart = 0;
   int blockLength = 0;
   for(int n = head; n < BLOCK_COUNT; n++) {
      if(n%32 == 0 && block_bitmap[n/32] == 0xFFFFFFFF) { n += 31; blockLength = 0; continue; }
      if(block_used(n)) { blockLength = 0; continue; }
      if(blockLength++ == 0) blockStart = n;
      if(blockLength == noBlocks) {
         set_blocks(blockStart, noBlocks, true, type);
         if(blockStart == head) head = next_free(blockStart + noBlocks);
         return (void*)(HEAP_KERNEL + blockStart*PAGE_SIZE);
      }
   }
   return NULL;
}

static void *malloc_small(int bytes) {
   if(bytes <= 0) return NULL;
   if(bytes > 256)
      return kalloc(&mem512_cache);
   if(bytes > 128)
      return kalloc(&mem256_cache);
   if(bytes > 64)
      return kalloc(&mem128_cache);
   if(bytes > 32)
      return kalloc(&mem64_cache);
   if(bytes > 16)
      return kalloc(&mem32_cache);
   return kalloc(&mem16_cache);
}

void *kmalloc(int bytes) {
   if(bytes <= 512)
      return malloc_small(bytes);

   return kmalloc_type(bytes, MEM_HEAP);
}

// guaranteed to be aligned
void *page_alloc(int bytes) {
   return kmalloc_type(bytes, MEM_HEAP);
}

void *resize(uint32_t offset, int oldsize, int newsize) {
   uint8_t *newaddr = kmalloc(newsize);
   if(newaddr == NULL)
      return NULL;
   uint8_t *oldaddr = (uint8_t *)offset;
   memcpy(newaddr, oldaddr, oldsize < newsize ? oldsize : newsize);
   kfree(oldaddr);
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

// utility

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
