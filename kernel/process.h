/* =============================================================================
 * SENG21213-OS :: Process Control Block
 * File   : kernel/process.h
 * ============================================================================= */
#ifndef PROCESS_H
#define PROCESS_H

#include <types.h>

#define MAX_PROCESSES  16
#define STACK_SIZE     4096   /* 4 KB per process stack */

typedef enum {
    PROCESS_FREE       = 0,
    PROCESS_READY      = 1,
    PROCESS_RUNNING    = 2,
    PROCESS_BLOCKED    = 3,
    PROCESS_TERMINATED = 4
} process_state_t;

typedef struct {
    uint32_t        pid;
    process_state_t state;
    uint32_t        esp;          /* saved stack pointer          */
    uint32_t       *stack;        /* base of allocated stack      */
    char            name[32];
} pcb_t;

/* The process table — accessible by scheduler */
extern pcb_t process_table[MAX_PROCESSES];
extern uint32_t current_pid;

void     process_init(void);
int32_t  process_create(const char *name, void (*entry)(void));
void     process_terminate(uint32_t pid);

#endif
