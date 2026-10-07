#ifndef DMA_H
#define DMA_H

#include <stdint.h>
#include <stddef.h>

#include "kobj.h"
#include "tasks.h"

typedef struct dma_t {
   uint32_t paddr;
   int size;
} dma_t;

typedef struct dma_ref_t {
   uint32_t vaddr; // unused - identity mapped
   int mapped_pages;
} dma_ref_t;

static inline dma_t *get_dma(kobj_inst_t *inst) {
   return inst->data;
}

kobj_handle_t dma_create(int size, void *process, uint32_t *vaddr);
void dma_cleanup(void *data);
void dma_close(void *ref_data, void *process, bool ending);

#endif