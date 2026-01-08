#ifndef EVENTS_H
#define EVENTS_H

#include <stddef.h>
#include "registers_t.h"

typedef struct event_t {
    int time;
    void *msg;
    struct event_t *next;
    void *callback;
    int task;
} __attribute__((packed)) event_t;

void events_add(int delta, void *callback, void *msg, int taskid);
void events_check(registers_t *regs);

#endif