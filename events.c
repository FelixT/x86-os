#include "memory.h"
#include "events.h"
#include "windowmgr.h"
#include "tasks.h"

bool events_active = false;

// events queue
event_t *first_event = NULL;

extern int timer_i;

void events_add(int delta, void* callback, int taskid) {
    if(!events_active) {
        events_active = true;
    }

    event_t *event = malloc(sizeof(event));
    event->time = (timer_i + delta)%10000000;
    event->callback = callback;
    event->task = taskid;
    event->next = NULL;

    // find place for it
    // time til event = (event.time - timer_i)%10000000

    // If no first event, this is it
    if(first_event == NULL) {
        first_event = event;
        return;
    }

    // If sooner than first, put this as first
    if(delta < (first_event->time - timer_i)%10000000) {
        event->next = first_event;
        first_event = event;
        return;
    }

    // If sooner than next, put this as next 
    event_t *e = first_event;
    while(e->next != NULL) {

        if(delta < (((event_t*)(e->next))->time - timer_i)%10000000) {
            event->next = ((event_t*)(e->next))->next;
            e->next = event;
            return;
        }
        e = e->next;
    }

    // Otherwise, last
    e->next = event;
}

void events_check(registers_t *regs) {
    if(!events_active) return;

    while(first_event != NULL) {
        
        if(timer_i >= first_event->time) {
            // skip if task is ended
            if(first_event->task >= 0
            && !gettasks()[first_event->task].enabled) {
                debug_writestr("Event is from an ended task\n");
                event_t *next = first_event->next;
                free((uint32_t)first_event, sizeof(event_t));
                continue;
            }

            // fire the event
            if(first_event->callback != NULL && first_event->task >= 0) {
                if(switch_to_task(first_event->task, regs)) {
                    task_call_subroutine(regs, (uint32_t)(first_event->callback), NULL, 0);
                }
            // todo: call function as kernel if task is -1
            }

            event_t *next = first_event->next;
            free((uint32_t)first_event, sizeof(event_t));

            first_event = next;
        } else {
            return;
        }
    }
}