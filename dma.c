#include "dma.h"
#include "windowmgr.h"

// dma - implement as a wrapper around kobjs

bool dma_map(kobj_inst_t *inst, int index, kobj_mapping_t *mapping) {
   if(index != 0) return false;
   dma_t *dma = inst->data;
   mapping->paddr = dma->paddr;
   mapping->no_cache = 0;
   mapping->size = dma->size;
   return true;
}

kobj_handle_t dma_create(int size, process_t *process, uint32_t *paddr) {
   if(size <= 0) return -1;
   uint8_t *mem = page_alloc(size); // use kmalloc page allocator as it guarantees continous mem
   dma_t *dma = kmalloc(sizeof(dma_t));
   kobj_inst_t *inst = dma ? create_kobj_inst(KOBJ_DMA, dma) : NULL;
   if(!mem || !dma || !inst) {
      debug_printf("dma_create failed: ran out of mem\n");
      if(inst) kfree(inst);
      if(mem) kfree(mem);
      if(dma) kfree(dma);
      return -1;
   }
   memset(mem, 0, size); // don't leak stale heap contents
   *paddr = (uint32_t)mem;
   dma->paddr = (uint32_t)mem;
   dma->size = size;
   
   kobj_handle_t h = process_acquire_kobj(process, inst);
   if(h < 0) {
      free_kobj_inst(inst); // dma_cleanup frees mem+dma_t
      return -1;
   }

   return h;
}

// called when dma instance is destroyed
void dma_cleanup(kobj_inst_t *inst) {
   dma_t *dma = inst->data;
   if(dma->size)
      kfree((void*)dma->paddr);
   kfree(dma);
}
