#ifndef KQUEUE_H
#define KQUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include "memory.h"

// generic kernel ring buffer handling
typedef struct kqueue_t {
   void *buffer;
   int capacity;
   int elem_size;
   int head, tail;
} kqueue_t;

// head, tail wrap at 2*capacity

static inline int kqueue_count(kqueue_t *q) {
   int n = q->tail - q->head;
   return n < 0 ? n + 2 * q->capacity : n;
}

static inline bool kqueue_empty(kqueue_t *q) {
   return q->tail == q->head;
}

static inline bool kqueue_full(kqueue_t *q) {
   return kqueue_count(q) == q->capacity;
}

// get slot from index (head/tail)
static inline int kqueue_slot(kqueue_t *q, int i) {
   return i >= q->capacity ? i - q->capacity : i;
}

static inline int kqueue_advance(kqueue_t *q, int i) {
   i++;
   if(i >= 2 * q->capacity)
      i = 0;
   return i;
}

static inline void *kqueue_get(kqueue_t *q, int i) {
   return (uint8_t*)q->buffer + kqueue_slot(q, i) * q->elem_size;
}

// last pushed (caller checks empty)
static inline void *kqueue_last(kqueue_t *q) {
   int i = q->tail == 0 ? 2 * q->capacity - 1 : q->tail - 1;
   return kqueue_get(q, i);
}

// first in queue (head or NULL)
static inline void *kqueue_peek(kqueue_t *q) {
   if(kqueue_empty(q)) return NULL;
   return kqueue_get(q, q->head);
}

// advance head
static inline void kqueue_drop(kqueue_t *q) {
   q->head = kqueue_advance(q, q->head);
}

// get element nth from head
static inline void *kqueue_at(kqueue_t *q, int n) {
   int i = q->head + n;
   if(i >= 2 * q->capacity) i -= 2 * q->capacity;
   return kqueue_get(q, i);
}

// advance tail, claim tail slot
static inline void *kqueue_reserve(kqueue_t *q) {
   if(kqueue_full(q)) return NULL;
   int tail = q->tail;
   q->tail = kqueue_advance(q, q->tail);
   return kqueue_get(q, tail);
}

static inline bool kqueue_push(kqueue_t *q, void *elem) {
   void *slot = kqueue_reserve(q);
   if(!slot) return false; // full
   memcpy_fast(slot, elem, q->elem_size);
   return true;
}

static inline bool kqueue_pop(kqueue_t *q, void *out) {
   if(kqueue_empty(q)) return false;
   void *elem = kqueue_get(q, q->head);
   memcpy_fast(out, elem, q->elem_size);
   q->head = kqueue_advance(q, q->head);
   return true;
}

static inline void kqueue_init(kqueue_t *q, void *buffer, int capacity, int elem_size) {
   q->elem_size = elem_size;
   q->capacity = capacity;
   q->head = 0;
   q->tail = 0;
   q->buffer = buffer;
}

#endif