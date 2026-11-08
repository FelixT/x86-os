#ifndef KALLOC_H
#define KALLOC_H

#include "paging.h"

// slab allocator
// 1 slab = 1 page of memory total

#define KSLAB_MIN_OBJ_SIZE 16 // must be >= 4, lower sizes increase header size
#define KSLAB_BITMAP_WORDS ((PAGE_SIZE/KSLAB_MIN_OBJ_SIZE + 31)/32)

typedef struct kslab_t kslab_t;

// keeps track of slabs
typedef struct kcache_t {
   //kslab_t *first;
   kslab_t *full, *partial, *empty;
   int slab_count; // number of pages/slabs
   int obj_size; // >= KSLAB_MIN_OBJ_SIZE, set to aligned size
   int obj_count; // total number of the object that can fit in a single slab (calculated once)
   uint32_t obj_start; // offset into slab of first object (aligned)
   int align; // required alignment in bytes (defaults to 4 for 32bit), must be power of two
} kcache_t;

typedef struct kslab_t {
   void *first_free; // points to first free slot. free slots contains pointer to next
   int allocated; // number of allocated obj in slab
   kcache_t *cache;

   kslab_t *prev, *next;

   uint32_t alloc_bitmap[KSLAB_BITMAP_WORDS]; // used for debugging/catching double free or invalid mem
} kslab_t;

static inline kslab_t *get_slab(uint32_t addr) {
   // header at start of page
   return (kslab_t*)page_align_down(addr);
}

static inline uint32_t align_up(uint32_t x, uint32_t a) {
   return (x + a - 1) & ~(a - 1);
}

static void list_push(kslab_t **head, kslab_t *s) {
   s->prev = NULL;
   s->next = *head;
   if(*head)
      (*head)->prev = s;
   *head = s;
}

static void list_remove(kslab_t **head, kslab_t *s) {
   if(s->prev)
      s->prev->next = s->next;
   else
      *head = s->next;
   if(s->next)
      s->next->prev = s->prev;
   s->next = s->prev = NULL;
}

static inline void kalloc_init(kcache_t *cache, int align, int obj_size) {
   if(obj_size < KSLAB_MIN_OBJ_SIZE)
      obj_size = KSLAB_MIN_OBJ_SIZE;
   cache->align = align;
   cache->obj_start = align_up(sizeof(kslab_t), align);
   cache->obj_size = align_up(obj_size, align);
   cache->obj_count =  (PAGE_SIZE - cache->obj_start)/cache->obj_size;
   cache->slab_count = 0;
   cache->full = NULL;
   cache->partial = NULL;
   cache->empty = NULL;
}

static inline kslab_t *kslab_create(kcache_t *cache) {
   void *page = kmalloc_type(PAGE_SIZE, MEM_SLAB);
   if(!page) return NULL;
   kslab_t *slab = page;
   slab->cache = cache;
   slab->first_free = page + cache->obj_start;
   slab->prev = NULL;
   slab->next = NULL;
   slab->allocated = 0;
   for(int i = 0; i < KSLAB_BITMAP_WORDS; i++)
      slab->alloc_bitmap[i] = 0;
   // populate free chain
   void *free = slab->first_free;
   for(int i = 0; i < cache->obj_count - 1; i++) {
      *((uint32_t*)free) = (uint32_t)free + cache->obj_size;
      free += cache->obj_size;
   }
   *((uint32_t*)free) = 0; // last points to NULL
   cache->slab_count++;
   return slab;
}

static inline uint32_t kslab_obj_index(kslab_t *slab, void *obj) {
   return ((uint32_t)obj - (uint32_t)slab - slab->cache->obj_start)/slab->cache->obj_size;
}

static inline void *kalloc(kcache_t *cache) {
   kslab_t *slab = cache->partial;
   if(!slab) {
      slab = cache->empty;
      if(slab) {
         list_remove(&cache->empty, slab);
      } else {
         slab = kslab_create(cache);
         if(!slab) return NULL;
      }
      list_push(&cache->partial, slab);
   }
   void *mem = slab->first_free;
   slab->first_free = (void*)(*(uint32_t*)slab->first_free); // follow free chain

   uint32_t index = kslab_obj_index(slab, mem);
   slab->alloc_bitmap[index/32] |= 1 << (index%32);
   slab->allocated++;
   if(slab->allocated == cache->obj_count) {
      // full
      list_remove(&cache->partial, slab);
      list_push(&cache->full, slab);
   }

   return mem;
}

// check for double free
static inline bool kslab_is_allocated(void *obj) {
   kslab_t *slab = get_slab((uint32_t)obj);
   kcache_t *cache = slab->cache;

   uint32_t offset = (uint32_t)obj - (uint32_t)slab;
   if(offset < cache->obj_start) return false; // in slab header
   offset -= cache->obj_start;
   if(offset % cache->obj_size != 0) return false; // only match if obj is start of object
   if(offset / cache->obj_size >= (uint32_t)cache->obj_count) return false; // after slab objects

   uint32_t index = offset / cache->obj_size;
   return slab->alloc_bitmap[index/32] & (1 << (index%32));
}

static inline bool kdealloc(void *obj) {
   kslab_t *slab = get_slab((uint32_t)obj);
   kcache_t *cache = slab->cache;

   uint32_t index = kslab_obj_index(slab, obj);
   slab->alloc_bitmap[index/32] &= ~(1 << (index%32));

   // swap
   *((uint32_t*)obj) = (uint32_t)slab->first_free;
   slab->first_free = obj;

   bool was_full = slab->allocated == cache->obj_count;
   slab->allocated--;
   if(was_full) {
      list_remove(&cache->full, slab);
      list_push(&cache->partial, slab);
   }
   if(slab->allocated == 0) {
      list_remove(&cache->partial, slab);
      if(cache->empty) {
         cache->slab_count--;
         return true; // free slab page
      }
      list_push(&cache->empty, slab);
   }
   return false;
}

#endif