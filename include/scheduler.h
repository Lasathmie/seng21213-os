#ifndef SCHEDULER_H
#define SCHEDULER_H

void scheduler_tick(void);
extern void context_switch(uint32_t *old_esp, uint32_t new_esp);

#endif