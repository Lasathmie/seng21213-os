#include "thread.h"
#include "process.h"

tid_t thread_create(const char *name, void (*fn)(void))
{
    return process_create(name, fn);
}

void thread_exit(void)
{
    process_terminate(current_pid);

    /* We're marked TERMINATED now, so scheduler_tick() will never pick
     * us again. Idle safely until the next timer tick switches us away
     * for good — interrupts are on, so this isn't a busy spin. */
    for (;;) {
        __asm__ volatile("hlt");
    }
}