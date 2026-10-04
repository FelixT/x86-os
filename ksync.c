#include "ksync.h"
#include "windowmgr.h"
#include "interrupts.h"

// kernel sync primitives

// allow multiple tasks to be parked waiting for a mutex to be released

static void kmutex_wait(kmutex_t *mutex) {
   // add to waiters queue
   task_state_t *task = get_current_task_state();
   task->mutex = mutex;
   task->mutex_next_waiter = NULL;
   if(mutex->waiters_tail)
      mutex->waiters_tail->mutex_next_waiter = task;
   else
      mutex->waiters = task;
   mutex->waiters_tail = task;

   debug_printf("kmutex_wait: pausing task\n");
   task_pause(task, PAUSE_KSYNC);
   kernel_block();
   debug_printf("kmutex_wait: resumed\n");
   while(task->mutex) {
      debug_printf("kmutex_wait: resumed outside of ksync\n");
      task_pause(task, PAUSE_KSYNC);
      kernel_block();
   }
}

static bool kmutex_wake(kmutex_t *mutex, bool wake) {
   // wake/unblock first task in queue
   task_state_t *task = mutex->waiters;
   if(!task) return false;

   mutex->waiters = mutex->waiters->mutex_next_waiter;
   if(!mutex->waiters)
      mutex->waiters_tail = NULL;

   task->mutex = NULL;
   task->mutex_next_waiter = NULL;
   task_resume(task);

   mutex->owner = task;
   mutex->owned_next = task->owned_mutexes;
   task->owned_mutexes = mutex;

   if(wake) {
      // immediately resume (yields current task)
      kernel_yield_to(task->task_id);
   }
   
   return true;
}

static void kmutex_release(task_state_t *task, kmutex_t *mutex, bool wake) {
   // remove from owner's owned list
   kmutex_t *prev = NULL;
   kmutex_t *cur = task->owned_mutexes;
   while(cur && cur != mutex) {
      prev = cur;
      cur = cur->owned_next;
   }
   if(cur) {
      if(prev)
         prev->owned_next = mutex->owned_next;
      else
         task->owned_mutexes = mutex->owned_next;
   }
   mutex->owned_next = NULL;

   mutex->owner = NULL;
   kmutex_wake(mutex, wake);
}

void kmutex_lock(kmutex_t *mutex) {
   // claim mutex, pause if already owned
   task_state_t *task = get_current_task_state();
   if(!task->in_syscall) {
      debug_printf("kmutex_lock: task %i not in syscall\n", task->task_id); // attempted lock from IRQ...
      kernel_panic();
   }
   if(mutex->owner == task) {
      debug_printf("kmutex_lock: task can't wait on mutex it owns\n");
      kernel_panic(); // would cause deadlock
   }
   if(!mutex->owner) {
      // uncontended, claim immediately
      mutex->owner = task;
      mutex->owned_next = task->owned_mutexes;
      task->owned_mutexes = mutex;
      return;
   }
   while(mutex->owner != task)
      kmutex_wait(mutex); // release/wake hands ownership to waiter
}

bool kmutex_unlock(kmutex_t *mutex) {
   task_state_t *task = get_current_task_state();
   if(mutex->owner != task) return false;
   kmutex_release(task, mutex, true);
   return true;
}

void ksync_cleanup_task(task_state_t *task) {
   // remove from waiters queue
   kmutex_t *mutex = task->mutex;
   if(mutex) {
      task_state_t *prev = NULL;
      task_state_t *cur = mutex->waiters;
      while(cur && cur != task) {
         prev = cur;
         cur = cur->mutex_next_waiter;
      }
      if(cur) {
         if(prev)
            prev->mutex_next_waiter = task->mutex_next_waiter;
         else
            mutex->waiters = task->mutex_next_waiter;
         if(mutex->waiters_tail == task)
            mutex->waiters_tail = prev;
      }
      task->mutex = NULL;
      task->mutex_next_waiter = NULL;
   }

   // todo: finish kernel task (therefore free held mutexes) before ending
   while(task->owned_mutexes) {
      debug_printf("ksync: releasing mutex held by ended task %i\n", task->task_id);
      kmutex_release(task, task->owned_mutexes, false); // don't yield (avoid task switch when ending task)
   }
}
