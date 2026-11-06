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

kobj_handle_t dma_create(int size, process_t *process, uint32_t *paddr);
void dma_cleanup(kobj_inst_t *inst);
bool dma_map(kobj_inst_t *inst, int index, kobj_mapping_t *mapping);

#endif