#include "dma.h"
#include "windowmgr.h"

// dma - implement as a wrapper around kobjs

// creates a dma inst + a ref to it
// returns handle to ref + addr
kobj_handle_t dma_create(int size, process_t *process, uint32_t *vaddr) {
   if(size <= 0) return -1;
   uint8_t *mem = malloc(size); // use kmalloc page allocator as it guarantees continous mem
   dma_t *dma = malloc(sizeof(dma_t));
   dma_ref_t *dma_ref = malloc(sizeof(dma_ref_t));
   kobj_inst_t *inst = dma ? create_kobj_inst(KOBJ_DMA, dma) : NULL;
   if(!mem || !dma || !dma_ref || !inst) {
      debug_printf("dma_create failed: ran out of mem\n");
      if(inst) free((uint32_t)inst, sizeof(kobj_inst_t));
      if(mem) free((uint32_t)mem, size);
      if(dma_ref) free((uint32_t)dma_ref, sizeof(dma_ref_t));
      if(dma) free((uint32_t)dma, sizeof(dma_t));
      return -1;
   }
   memset(mem, 0, size); // don't leak stale heap contents
   dma->paddr = (uint32_t)mem;
   dma->size = size;
   
   kobj_handle_t h = process_acquire_kobj(process, inst);
   if(h < 0) {
      free((uint32_t)dma_ref, sizeof(dma_ref_t));
      free_kobj_inst(inst); // dma_cleanup frees mem+dma_t
      return -1;
   }
   kobj_ref_t *ref = process_get_kobj(process, h);
   ref->data = dma_ref;

   // identity map - todo: give process vaddr
   dma_ref->mapped_pages = map_size(process->page_dir, dma->paddr, dma->paddr, size, 1, 1, 0);
   process->no_allocated += dma_ref->mapped_pages;
   dma_ref->vaddr = dma->paddr;

   *vaddr = dma_ref->vaddr;

   return h;
}

// called when dma instance is destroyed
void dma_cleanup(kobj_inst_t *inst) {
   dma_t *dma = inst->data;
   if(dma->size)
      free(dma->paddr, dma->size);
   free((uint32_t)dma, sizeof(dma_t));
}

// called when ref is destroyed
void dma_close(kobj_ref_t *ref, process_t *process, bool ending) {
   dma_t *dma = ref->inst->data;

   if(ref->data)
      free((uint32_t)ref->data, sizeof(dma_ref_t));
   
   if(ending)
      return; // don't bother unmapping when process is ending

   // unmap heap from user (still needs to be identity mapped for kernel)
   process->no_allocated -= map_size(process->page_dir, dma->paddr, dma->paddr, dma->size, 0, 1, 0);
}
