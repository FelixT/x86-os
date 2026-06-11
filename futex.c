#include "futex.h"

// futex for synchronisation between threads of the same process

typedef struct futex_waiter_t {
   int task_id;
   process_t *process;
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
   if(!copy_from_task(task_id, &val, futex_addr, sizeof(uint32_t))) {
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
   waiter->process = get_current_task_state()->process;
   waiter->futex_addr = futex_addr;

   // add to start of waiters bucket list
   struct futex_bucket *bucket = &buckets[private_hash((uintptr_t)futex_addr)];
   waiter->next = bucket->w;
   bucket->w = waiter;

   return FUTEX_WAIT_BLOCK;
}

void futex_wake(void *regs, void *futex_addr) {
   unsigned hash = private_hash((uintptr_t)futex_addr);
   futex_waiter_t *prev = NULL;
   futex_waiter_t *waiter = buckets[hash].w;
   while(waiter != NULL) {
      if(waiter->futex_addr != futex_addr || waiter->process != get_current_task_state()->process) {
         prev = waiter;
         waiter = waiter->next;
         continue;
      }
      // wake this task
      gettasks()[waiter->task_id].paused = false;
      switch_to_task(waiter->task_id, regs);
      // remove from waiters list
      if(prev)
         prev->next = waiter->next;
      else
         buckets[hash].w = waiter->next;
      free((uint32_t)waiter, sizeof(futex_waiter_t));
      return;
   }
}