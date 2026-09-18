#ifndef MUTEX_H
#define MUTEX_H

#include <types.h>
#include "process.h"

typedef struct {
    volatile int locked;                  /* 0 = free, 1 = held */
    int32_t      owner;                   /* pid of holder, -1 if free */
    uint32_t     waiters[MAX_PROCESSES];  /* blocked pid queue (FIFO) */
    int          nwaiters;
} mutex_t;

void mutex_init  (mutex_t *m);
void mutex_lock  (mutex_t *m);
void mutex_unlock(mutex_t *m);

#endif