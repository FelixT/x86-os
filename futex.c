#include "futex.h"
#include "shared.h"

// futex for synchronisation

typedef struct futex_waiter_t {
   int task_id;
   uint32_t task_uid;
   uint32_t process_uid; // used to compare private futex, 0 for shared memory
   uint32_t key_addr; // vaddr if private, paddr if shared
   struct futex_waiter_t *next;
} futex_waiter_t;

struct futex_bucket {
   futex_waiter_t *w;
} buckets[256];

static inline uint32_t private_hash(uint32_t k) {
   return (k >> 2) % 256;
}

static uint32_t futex_key(process_t *process, uint32_t vaddr, uint32_t *process_uid) {
   vm_region_t *region = process_find_vm(process, vaddr);
   if(region && region->inst->obj->type == KOBJ_SHARED) {
      shared_block_t *block = region->inst->data;
      *process_uid = 0;
      return block->paddr + (vaddr - region->vaddr);
   }
   *process_uid = process->uid;
   return vaddr;
}

int futex_wait(void *futex_addr, uint32_t expected) {
   // check if value matches expected
   uint32_t val;
   int task_id = get_current_task();
   if(copy_from_task(task_id, &val, futex_addr, sizeof(uint32_t)) != sizeof(uint32_t)) {
      return FUTEX_WAIT_FAIL;
   }
   if(val != expected) {
      // value doesn't match, return immediately
      return FUTEX_WAIT_NOBLOCK;
   }

   task_state_t *task = get_current_task_state();
   futex_waiter_t *waiter = kmalloc(sizeof(futex_waiter_t));
   if(!waiter) {
      return FUTEX_WAIT_FAIL;
   }
   waiter->task_id = task_id;
   waiter->task_uid = task->task_uid;

   uint32_t pid;
   uint32_t addr = futex_key(task->process, (uint32_t)futex_addr, &pid);
   waiter->key_addr = addr;
   waiter->process_uid = pid;

   // add to start of waiters bucket list
   struct futex_bucket *bucket = &buckets[private_hash(addr)];
   waiter->next = bucket->w;
   bucket->w = waiter;

   return FUTEX_WAIT_BLOCK;
}

void futex_wake(void *regs, void *futex_addr) {
   task_state_t *task = get_current_task_state(); // waker
   uint32_t pid;
   uint32_t addr = futex_key(task->process, (uint32_t)futex_addr, &pid);
   uint32_t hash = private_hash(addr);

   futex_waiter_t *prev = NULL;
   futex_waiter_t *waiter = buckets[hash].w;
   while(waiter != NULL) {
      task_state_t *waiter_task = &gettasks()[waiter->task_id];
      bool stale = !waiter_task->enabled || waiter_task->task_uid != waiter->task_uid;
      bool match = waiter->key_addr == addr && waiter->process_uid == pid;

      if(!stale && !match) {
         prev = waiter;
         waiter = waiter->next;
         continue;
      }

      futex_waiter_t *unlinked = waiter;
      if(prev)
         prev->next = waiter->next;
      else
         buckets[hash].w = waiter->next;
      waiter = waiter->next;
      kfree(unlinked);

      if(!stale && match) {
         task_resume(waiter_task);
         switch_to_task(waiter_task->task_id, regs);
         return; // wake only one waiter
      }
   }
}