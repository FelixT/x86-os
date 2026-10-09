#include "shared.h"
#include "memory.h"
#include "windowmgr.h"

// shared memory, uses global vmem space

uint32_t shared_uid_counter = 0;
shared_block_t *shared_blocks = NULL; // linked list

uint32_t shared_next = V_SHARED_START; // note: never reclaimed

bool shared_addr_accessible(process_t *process, uint32_t vaddr) {
   for(shared_block_t *b = shared_blocks; b; b = b->next) {
      if(vaddr < b->vaddr || vaddr >= b->vaddr + b->size)
         continue;
      for(shared_instance_t *i = b->instances; i; i = i->next)
         if(i->process == process)
            return true;
      return false;
   }
   return false;
}

uint32_t shared_map_instance(process_t *process, shared_block_t *block) {
   // check if already mapped by this process
   shared_instance_t *cur = block->instances;
   while(cur) {
      if(cur->process == process)
         return block->vaddr;
      cur = cur->next;
   }

   shared_instance_t *instance = (shared_instance_t *)malloc(sizeof(shared_instance_t));
   if(!instance)
      return 0;
   
   instance->process = process;
   instance->next = block->instances;
   block->instances = instance;

   map_size(process->page_dir, block->paddr, block->vaddr, block->size, 1, 1, 0);
   return block->vaddr;
}

bool shared_connect(kobj_ref_t *ref, process_t *process) {
   (void)ref;
   (void)process;
   return true; // mapping deferred to shared_map
}

kobj_handle_t shared_create(process_t *process, uint32_t size, uint32_t *vaddr) {
   size = ((size + (MEM_BLOCK_SIZE - 1)) / MEM_BLOCK_SIZE) * MEM_BLOCK_SIZE; // round up
   if(shared_next + size >= V_SHARED_END)
      return -1;

   shared_block_t *block = (shared_block_t *)malloc(sizeof(shared_block_t));
   if(!block)
      return -1;

   block->uid = shared_uid_counter++;
   block->size = size;
   block->paddr = (uint32_t)malloc(size);
   if(!block->paddr) {
      free((uint32_t)block, sizeof(shared_block_t));
      return -1;
   }
   block->vaddr = shared_next;
   block->instances = NULL;

   // create kobj
   kobj_inst_t *inst = create_kobj_inst(KOBJ_SHARED, block);
   if(!inst) {
      free(block->paddr, block->size);
      free((uint32_t)block, sizeof(shared_block_t));
      return -1;
   }
   kobj_handle_t h = process_acquire_kobj(process, inst);
   if(h < 0) {
      free_kobj_inst(inst);
      return -1;
   }

   if(!shared_map_instance(process, block)) {
      process_release_kobj(process, h);
      return -1;
   }

   block->next = shared_blocks;
   shared_blocks = block;
   shared_next += size;
   *vaddr = block->vaddr;
   return h;
}

void shared_free_inst(kobj_inst_t *inst) {
   shared_block_t *block = inst->data;
   free(block->paddr, block->size);
   // remove from blocks list
   if(block == shared_blocks) {
      shared_blocks = block->next;
   } else {
      shared_block_t *cur = shared_blocks;
      shared_block_t *prev = NULL;
      while(cur) {
         if(cur == block) {
            if(prev)
               prev->next = cur->next;
            break;
         }
         prev = cur;
         cur = cur->next;
      }
   }

   free((uint32_t)block, sizeof(shared_block_t));
}

void shared_detach(process_t *process, shared_block_t *block, bool do_unmap) {
   int unmap_blocks = (block->size+(MEM_BLOCK_SIZE-1))/MEM_BLOCK_SIZE;
   shared_instance_t *prev_instance = NULL;
   bool found = false;
   for(shared_instance_t *instance = block->instances; instance; prev_instance = instance, instance = instance->next) {
      if(instance->process != process) continue;
      found = true;
      // remove from list
      if(prev_instance)
         prev_instance->next = instance->next;
      else
         block->instances = instance->next;
      free((uint32_t)instance, sizeof(shared_instance_t));
      break;
   }

   if(found && do_unmap) {
      for(int i = 0; i < unmap_blocks; i++)
         unmap(process->page_dir, block->vaddr + (i*MEM_BLOCK_SIZE));
   }
}

void shared_close(kobj_ref_t *ref, process_t *process, bool ending) {
   if(process_has_another_ref(process, ref))
      return; // same memory block referenced by multiple handles owned by process
   shared_block_t *block = ref->inst->data;
   shared_detach(process, block, !ending);
}
