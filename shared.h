#ifndef SHARED_H
#define SHARED_H

#include "tasks.h"
#include "kobj.h"

typedef struct shared_block_t {
   uint32_t size;
   uint32_t paddr;
} shared_block_t;

kobj_handle_t shared_create(process_t *process, uint32_t size);
void shared_free_inst(kobj_inst_t *inst);
bool shared_map(kobj_inst_t *inst, int index, kobj_mapping_t *mapping);
bool shared_connect(kobj_ref_t *ref, process_t *process);

#endif