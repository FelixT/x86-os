#include "shared.h"
#include "memory.h"
#include "windowmgr.h"

// shared memory, uses global vmem space

uint32_t shared_uid_counter = 0;
shared_block_t *shared_blocks = NULL; // linked list

uint32_t shared_next = V_SHARED_START; // note: never reclaimed

bool shared_check_access(process_t *process, shared_block_t *block) {
   if(process->uid == block->owner_uid)
      return true;

   for(int i = 0; i < block->grant_count; i++) {
      if(process->uid == block->grants[i].uid)
         return true;
   }

   return false;
}

// number of unmapped grants in a block
// while pending grants exist the block is kept alive to allow owner to exit without waiting
int shared_pending_grants(shared_block_t *block) {
   int pending = 0;
   for(int i = 0; i < block->grant_count; i++) {
      if(!block->grants[i].consumed)
         pending++;
   }
   return pending;
}

// mark a process's grant consumed once it maps
void shared_consume_grant(process_t *process, shared_block_t *block) {
   for(int i = 0; i < block->grant_count; i++) {
      if(block->grants[i].uid == process->uid)
         block->grants[i].consumed = true;
   }
}

shared_block_t *shared_block_from_uid(uint32_t uid) {
   shared_block_t *block = shared_blocks;
   while(block) {
      if(block->uid == uid)
         return block;

      block = block->next;
   }
   return block;
}

bool shared_grant_access(process_t *process, uint32_t block_uid, uint32_t target_uid) {
   shared_block_t *block = shared_block_from_uid(block_uid);
   if(!block || process->uid != block->owner_uid || block->grant_count == SHARED_MAX_GRANTS)
      return false;

   block->grants[block->grant_count].uid = target_uid;
   block->grants[block->grant_count].consumed = false;
   block->grant_count++;
   return true;
}

bool shared_map_instance(process_t *process, shared_block_t *block) {
   if(!shared_check_access(process, block))
      return false;

   // check if already mapped by this process
   shared_instance_t *cur = block->instances;
   while(cur) {
      if(cur->process == process)
         return true;
      cur = cur->next;
   }

   shared_instance_t *instance = (shared_instance_t *)malloc(sizeof(shared_instance_t));
   if(!instance)
      return false;
   
   instance->process = process;
   instance->next = block->instances;
   block->instances = instance;
   shared_consume_grant(process, block);

   map_size(process->page_dir, block->paddr, block->vaddr, block->size, 1, 1);
   return true;
}

uint32_t shared_map_uid(process_t *process, uint32_t block_uid) {
   shared_block_t *block = shared_block_from_uid(block_uid);
   if(!block)
      return 0;

   if(!shared_map_instance(process, block))
      return 0;

   return block->vaddr;
}

shared_block_t *shared_create(process_t *process, uint32_t size) {
   size = ((size + (MEM_BLOCK_SIZE - 1)) / MEM_BLOCK_SIZE) * MEM_BLOCK_SIZE; // round up
   if(shared_next + size >= V_SHARED_END) {
      return NULL;
   }

   shared_block_t *block = (shared_block_t *)malloc(sizeof(shared_block_t));
   if(!block)
      return NULL;

   block->uid = shared_uid_counter++;
   block->size = size;
   block->paddr = (uint32_t)malloc(size);
   if(!block->paddr) {
      free((uint32_t)block, sizeof(shared_block_t));
      return NULL;
   }
   block->vaddr = shared_next;
   block->owner_uid = process->uid;
   block->instances = NULL;
   block->grant_count = 0;

   if(!shared_map_instance(process, block)) {
      free(block->paddr, block->size);
      free((uint32_t)block, sizeof(shared_block_t));
      return NULL;
   }
   block->next = shared_blocks;
   shared_blocks = block;
   shared_next += size;
   return block;
}

void shared_detach(process_t *process, shared_block_t *block, bool do_unmap) {
   int unmap_blocks = (block->size+(MEM_BLOCK_SIZE-1))/MEM_BLOCK_SIZE;
   shared_instance_t *prev_instance = NULL;
   for(shared_instance_t *instance = block->instances; instance; prev_instance = instance, instance = instance->next) {
      if(instance->process != process)
         continue;

      if(do_unmap) {
         for(int i = 0; i < unmap_blocks; i++)
            unmap(process->page_dir, block->vaddr + (i*MEM_BLOCK_SIZE));
      }
      // remove from list
      if(prev_instance)
         prev_instance->next = instance->next;
      else
         block->instances = instance->next;
      free((uint32_t)instance, sizeof(shared_instance_t));
      break;
   }

   // free the block when nothing maps it and no outstanding grant exists
   if(!block->instances && shared_pending_grants(block) == 0) {
      // remove from list
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
      free(block->paddr, block->size);
      free((uint32_t)block, sizeof(shared_block_t));
   }
}

bool shared_close(process_t *process, uint32_t uid) {
   shared_block_t *block = shared_block_from_uid(uid);
   if(!block)
      return false;

   shared_detach(process, block, true);
   return true;
}

void shared_cleanup(process_t *process) {
   shared_block_t *block = shared_blocks;
   while(block) {
      shared_block_t *next = block->next;
      shared_detach(process, block, false);
      block = next;
   }
}