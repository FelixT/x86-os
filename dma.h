#ifndef DMA_H
#define DMA_H

#include <stdint.h>
#include <stddef.h>

#include "kobj.h"
#include "tasks.h"

typedef struct process_t process_t;

typedef struct dma_t {
   uint32_t paddr;
   int size;
} dma_t;

typedef struct dma_ref_t {
   uint32_t vaddr; // unused - identity mapped
   int mapped_pages;
} dma_ref_t;

kobj_handle_t dma_create(int size, process_t *process, uint32_t *vaddr);
void dma_cleanup(kobj_inst_t *inst);
void dma_close(kobj_ref_t *ref, process_t *process, bool ending);

#endif