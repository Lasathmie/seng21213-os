#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <types.h>
#include "process.h"

void     scheduler_init(void);
void     scheduler_tick(void);          /* called from IRQ0 handler */
void     scheduler_yield(void);         /* voluntarily give up the CPU now */
void     scheduler_unblock(uint32_t pid); /* BLOCKED -> READY */
pcb_t   *scheduler_get_current(void);

#endif