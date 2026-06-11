#ifndef FUTEX_H
#define FUTEX_H

#include "tasks.h"

#define FUTEX_WAIT_NOBLOCK 0
#define FUTEX_WAIT_BLOCK -1
#define FUTEX_WAIT_FAIL -2

int futex_wait(void *futex_addr, uint32_t expected);
void futex_wake(void *regs, void *futex_addr);

#endif