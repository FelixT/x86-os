#include "shared.h"
#include "memory.h"
#include "windowmgr.h"

uint32_t shared_map(process_t *process, kobj_inst_t *inst) {
   shared_block_t *block = inst->data;
   vm_region_t *region = process_alloc_vm(process, block->size, 1, inst);
   if(!region) return 0;

   if(map_size(process->page_dir, block->paddr, region->vaddr, region->size, 1, 1, 0) != (int)page_align_up(block->size)/PAGE_SIZE) {
      process_unmap_vm(process, region->vaddr);
      return 0;
   }
   return region->vaddr;
}

bool shared_connect(kobj_ref_t *ref, process_t *process) {
   (void)ref;
   (void)process;
   return true; // mapping deferred to shared_map
}

kobj_handle_t shared_create(process_t *process, uint32_t size) {
   size = page_align_up(size);
   if(size == 0) return -1;
   if(size > VM_END - VM_START) return -1;

   shared_block_t *block = (shared_block_t *)malloc(sizeof(shared_block_t));
   if(!block)
      return -1;

   block->size = size;
   block->paddr = (uint32_t)malloc(size);
   if(!block->paddr) {
      free((uint32_t)block, sizeof(shared_block_t));
      return -1;
   }
   memset((void*)block->paddr, 0, size); // zero memory

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

   return h;
}

void shared_free_inst(kobj_inst_t *inst) {
   shared_block_t *block = inst->data;
   free(block->paddr, block->size);
   free((uint32_t)block, sizeof(shared_block_t));
}
