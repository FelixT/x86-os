#ifndef SHARED_H
#define SHARED_H

// shared memory
// defines linked list of shared memory blocks

#include "tasks.h"
#include "kobj.h"

typedef struct shared_instance_t {
   process_t *process;
   struct shared_instance_t *next;
} shared_instance_t;

typedef struct shared_block_t {
   uint32_t uid;
   uint32_t size;
   uint32_t paddr;
   uint32_t vaddr;
   shared_instance_t *instances;
   struct shared_block_t *next;
} shared_block_t;

bool shared_addr_accessible(process_t *process, uint32_t vaddr);
kobj_handle_t shared_create(process_t *process, uint32_t size, uint32_t *vaddr);
void shared_close(kobj_ref_t *ref, process_t *process, bool ending);
void shared_free_inst(kobj_inst_t *inst);
uint32_t shared_map_instance(process_t *process, shared_block_t *block);
bool shared_connect(kobj_ref_t *ref, process_t *process);

#endif