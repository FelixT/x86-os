#ifndef SHARED_H
#define SHARED_H

// shared memory
// defines linked list of shared memory blocks

#include "tasks.h"

#define SHARED_MAX_GRANTS 8

typedef struct shared_instance_t {
   process_t *process;
   struct shared_instance_t *next;
} shared_instance_t;

typedef struct shared_grant_t {
   uint32_t uid; // process uid with access
   bool consumed; // has this uid mapped the block
} shared_grant_t;

typedef struct shared_block_t {
   uint32_t uid;
   uint32_t size;
   uint32_t paddr;
   uint32_t vaddr;
   uint32_t owner_uid; // process uid
   shared_instance_t *instances;
   shared_grant_t grants[SHARED_MAX_GRANTS]; // uids with access
   int grant_count;
   struct shared_block_t *next;
} shared_block_t;

bool shared_grant_access(process_t *process, uint32_t block_uid, uint32_t target_uid);
uint32_t shared_map_uid(process_t *process, uint32_t block_uid);
shared_block_t *shared_create(process_t *process, uint32_t size);
bool shared_close(process_t *process, uint32_t uid);
void shared_cleanup(process_t *process);

#endif