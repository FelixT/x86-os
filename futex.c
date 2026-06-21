#include "futex.h"

// futex for synchronisation between threads of the same process

typedef struct futex_waiter_t {
   int task_id;
   uint32_t task_uid;
   uint32_t process_uid;
   void *futex_addr;
   struct futex_waiter_t *next;
} futex_waiter_t;

struct futex_bucket {
   futex_waiter_t *w;
} buckets[256];

static inline unsigned private_hash(uintptr_t k) {
   return (k >> 2) % 256;
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

   futex_waiter_t *waiter = malloc(sizeof(futex_waiter_t));
   if(!waiter) {
      return FUTEX_WAIT_FAIL;
   }
   waiter->task_id = task_id;
   waiter->task_uid = get_current_task_state()->task_uid;
   waiter->process_uid = get_current_task_state()->process->uid;
   waiter->futex_addr = futex_addr;

   // add to start of waiters bucket list
   struct futex_bucket *bucket = &buckets[private_hash((uintptr_t)futex_addr)];
   waiter->next = bucket->w;
   bucket->w = waiter;

   return FUTEX_WAIT_BLOCK;
}

void futex_wake(void *regs, void *futex_addr) {
   unsigned hash = private_hash((uintptr_t)futex_addr);
   uint32_t waker_process_uid = get_current_task_state()->process->uid;
   futex_waiter_t *prev = NULL;
   futex_waiter_t *waiter = buckets[hash].w;
   while(waiter != NULL) {
      task_state_t *task = &gettasks()[waiter->task_id];
      bool stale = !task->enabled || task->task_uid != waiter->task_uid;
      bool match = waiter->futex_addr == futex_addr && waiter->process_uid == waker_process_uid;

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
      free((uint32_t)unlinked, sizeof(futex_waiter_t));

      if(!stale && match) {
         task->paused = false;
         switch_to_task(task->task_id, regs);
         return; // wake only one waiter
      }
   }
}