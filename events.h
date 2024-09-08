#ifndef EVENTS_H
#define EVENTS_H

#include <stddef.h>
#include "registers_t.h"

typedef struct {
    int time;
    char *msg;
    void *next;
    void *callback;
    int task;
} __attribute__((packed)) event_t;

void events_add(int delta, void* callback, int taskid);
void events_check(registers_t *regs);

#endif