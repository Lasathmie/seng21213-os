#include "mutex.h"
#include "scheduler.h"

/* XCHG is implicitly atomic on x86 (asserts LOCK# on the bus) — this is
 * the one instruction in this whole file that actually needs to be a
 * single indivisible test-and-set. */
static inline int atomic_xchg(volatile int *ptr, int val)
{
    int old;
    __asm__ volatile("xchgl %0, %1"
        : "=r"(old), "+m"(*ptr)
        : "0"(val) : "memory");
    return old;
}

void mutex_init(mutex_t *m)
{
    m->locked   = 0;
    m->owner    = -1;
    m->nwaiters = 0;
}

void mutex_lock(mutex_t *m)
{
    while (atomic_xchg(&m->locked, 1) == 1) {
        /* Someone else holds it. Add ourselves to the waiter list and
         * mark BLOCKED — bracketed with cli/sti so a timer tick can't
         * land in the middle of this state change. */
        __asm__ volatile("cli");
        m->waiters[m->nwaiters++] = current_pid;
        process_table[current_pid].state = PROCESS_BLOCKED;
        __asm__ volatile("sti");

        scheduler_yield();   /* doesn't return until we're READY again */
        /* Woken up — loop back and try the xchg again. Someone else
         * may have grabbed it first; if so we block again. */
    }
    m->owner = (int32_t)current_pid;
}

void mutex_unlock(mutex_t *m)
{
    m->owner  = -1;
    m->locked = 0;

    if (m->nwaiters > 0) {
        uint32_t pid = m->waiters[--m->nwaiters];
        scheduler_unblock(pid);
    }
}