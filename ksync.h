#ifndef KSYNC_H
#define KSYNC_H

#include "tasks.h"

typedef struct kmutex_t {
    task_state_t *owner;
    task_state_t *waiters; // linked list
    task_state_t *waiters_tail;
    struct kmutex_t *owned_next; // next mutex in owner's owned list
} kmutex_t;

void kmutex_lock(kmutex_t *mutex);
bool kmutex_unlock(kmutex_t *mutex);
void ksync_cleanup_task(task_state_t *task);

#endif
