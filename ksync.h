#ifndef KSYNC_H
#define KSYNC_H

#include <stdbool.h>

typedef struct kmutex_t {
    struct task_state_t *owner;
    struct task_state_t *waiters; // linked list
    struct task_state_t *waiters_tail;
    struct kmutex_t *owned_next; // next mutex in owner's owned list
} kmutex_t;

void kmutex_lock(kmutex_t *mutex);
bool kmutex_unlock(kmutex_t *mutex);
bool kmutex_unlock_yield(kmutex_t *mutex);
void ksync_cleanup_task(struct task_state_t *task);

#endif
