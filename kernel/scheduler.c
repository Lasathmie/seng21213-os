#include "scheduler.h"
#include "pit.h"
#include "pic.h"

/* context_switch is defined in switch.asm
 * Signature: void context_switch(uint32_t *old_esp, uint32_t new_esp)
 * It pushes/saves current ESP into *old_esp, loads new_esp, then does
 * POPAD + RET into the incoming process. Its own `sti` at the end
 * guarantees interrupts are on again once we're running the new process.
 */
extern void context_switch(uint32_t *old_esp, uint32_t new_esp);

void scheduler_init(void)
{
    /* Nothing extra needed — process_init() already set current_pid = 0 */
}

/* pick_and_switch — round-robin scan for the next READY process and
 * switch to it. Shared by the timer path (scheduler_tick) and the
 * voluntary path (scheduler_yield) so they can never drift apart.
 *
 * Only flips the outgoing process RUNNING -> READY; a BLOCKED process
 * (e.g. waiting on a mutex) is left BLOCKED so it won't be picked again
 * until something explicitly unblocks it.
 *
 * Returns 1 if it switched, 0 if the current process is the only one
 * runnable (nothing to do).
 */
static int pick_and_switch(void)
{
    uint32_t old_pid  = current_pid;
    uint32_t next_pid = old_pid;

    for (uint32_t i = 1; i < MAX_PROCESSES; i++) {
        uint32_t candidate = (old_pid + i) % MAX_PROCESSES;
        if (process_table[candidate].state == PROCESS_READY) {
            next_pid = candidate;
            break;
        }
    }

    if (next_pid == old_pid) return 0;

    if (process_table[old_pid].state == PROCESS_RUNNING)
        process_table[old_pid].state = PROCESS_READY;

    process_table[next_pid].state = PROCESS_RUNNING;
    current_pid = next_pid;

    context_switch(&process_table[old_pid].esp, process_table[next_pid].esp);
    return 1;
}

/* scheduler_tick — called from irq0_handler (isr_stub.asm) on every timer tick */
void scheduler_tick(void)
{
    tick_count++;

    /* EOI must always be sent, whether or not we switch — NOT only
     * inside the switch branch. If the current process is ever the
     * only RUNNABLE one (everyone else BLOCKED on a mutex, say), a
     * conditional EOI here would never fire again and the timer
     * would go silent for good. */
    pic_send_eoi(0);

    pick_and_switch();
}

/* scheduler_yield — voluntarily give up the CPU right now instead of
 * waiting for the next timer tick. Used by mutex_lock() when it has
 * just marked the caller BLOCKED, so it doesn't busy-spin for up to
 * 10ms before actually being switched away.
 *
 * cli/sti bracket the switch so a timer tick can't land mid-transition
 * (e.g. between reading current_pid and updating process_table).
 */
void scheduler_yield(void)
{
    __asm__ volatile("cli");
    if (!pick_and_switch()) {
        /* nothing else runnable — nothing to yield to, just resume */
        __asm__ volatile("sti");
    }
    /* If it did switch, context_switch's own `sti` already re-enabled
     * interrupts by the time we're back here (possibly much later). */
}

/* scheduler_unblock — wake a process that's waiting on a mutex/semaphore.
 * Only moves it BLOCKED -> READY; the scheduler decides when it actually
 * runs. Safe to call from mutex_unlock()/sem_signal(). */
void scheduler_unblock(uint32_t pid)
{
    if (pid >= MAX_PROCESSES) return;
    if (process_table[pid].state == PROCESS_BLOCKED)
        process_table[pid].state = PROCESS_READY;
}

pcb_t *scheduler_get_current(void)
{
    return &process_table[current_pid];
}