#include "memory.h"
#include "events.h"
#include "windowmgr.h"
#include "tasks.h"

bool events_active = false;

// events queue
event_t *first_event = NULL;

extern int timer_i;

void events_add(int delta, void* callback, int taskid) {
    debug_writestr("Adding event for task ");
    debug_writeuint(taskid);
    debug_writestr("\n");

    if(!events_active) {
        debug_writestr("Events queue init\n");
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
        debug_writestr("First in queue\n");
        first_event = event;
        return;
    }

    // If sooner than first, put this as first
    if(delta < (first_event->time - timer_i)%10000000) {
            debug_writestr("sooner than first\n");
        event->next = first_event;
        first_event = event;
        return;
    }

    // If sooner than next, put this as next 
    event_t *e = first_event;
    while(e->next != NULL) {
        debug_writestr("looping\n");

        if(delta < (((event_t*)(e->next))->time - timer_i)%10000000) {
            debug_writestr("sooner than next\n");
            event->next = ((event_t*)(e->next))->next;
            e->next = event;
            return;
        }
        e = e->next;
    }

    // Otherwise, last
    debug_writestr("last\n");

    e->next = event;
}

void events_check(registers_t *regs) {
    if(!events_active) return;

    while(first_event != NULL) {
        
        if(timer_i == first_event->time) {
            // fire the event
            debug_writestr("Event hit\n");
            if(first_event->callback != NULL) {
                debug_writestr("Callback\n");
                // todo: check if task is still active
                if(first_event->task >= 0) {
                    debug_writestr("Switching task\n");
                    if(switch_to_task(first_event->task, regs)) {
                        debug_writestr("Calling subroutine\n");
                        task_call_subroutine(regs, (uint32_t)(first_event->callback), NULL, 0);
                    }
                }

                // todo: call function as kernel if task is -1
            }

            debug_writestr("Next event is ");
            debug_writeuint((uint32_t)first_event->next);
            debug_writestr("\n");

            event_t *next = first_event->next;
            //free((uint32_t)first_event, sizeof(event_t));

            first_event = next;
        } else {
            return;
        }
    }
}