#ifndef PROCESS_H
#define PROCESS_H
#include <stdint.h>

#define MAX_PROCS   16
#define STACK_SIZE  4096

typedef enum {
    PROC_UNUSED = 0,
    PROC_READY,
    PROC_RUNNING,
    PROC_BLOCKED,
    PROC_ZOMBIE,
} proc_state_t;

typedef struct {
    uint32_t      pid;
    proc_state_t  state;
    uint32_t      esp;
    uint32_t      stack_base;
    void        (*entry)(void);
    char          name[32];
    uint32_t      ticks;
} pcb_t;

extern pcb_t proc_table[MAX_PROCS];
extern int   current_proc;

void   process_init(void);
pcb_t *proc_create(const char *name, void (*entry)(void));
void   proc_exit(void);

#endif