#include "shared.h"
#include "memory.h"
#include "windowmgr.h"

bool shared_map(kobj_inst_t *inst, int index, kobj_mapping_t *mapping) {
   if(index != 0) return false;
   shared_block_t *block = inst->data;
   mapping->paddr = block->paddr;
   mapping->no_cache = 0;
   mapping->size = block->size;
   return true;
}

kobj_handle_t shared_create(process_t *process, uint32_t size) {
   size = page_align_up(size);
   if(size == 0) return -1;
   if(size > VM_END - VM_START) return -1;

   shared_block_t *block = (shared_block_t *)kmalloc(sizeof(shared_block_t));
   if(!block)
      return -1;

   block->size = size;
   block->paddr = (uint32_t)kmalloc(size);
   if(!block->paddr) {
      kfree(block);
      return -1;
   }
   memset((void*)block->paddr, 0, size); // zero memory

   // create kobj
   kobj_inst_t *inst = create_kobj_inst(KOBJ_SHARED, block);
   if(!inst) {
      kfree((void*)block->paddr);
      kfree(block);
      return -1;
   }
   kobj_handle_t h = process_acquire_kobj(process, inst, KOBJ_RIGHT_SAME);
   if(h < 0) {
      free_kobj_inst(inst);
      return -1;
   }

   return h;
}

void shared_free_inst(kobj_inst_t *inst) {
   shared_block_t *block = inst->data;
   kfree((void*)block->paddr);
   kfree(block);
}
