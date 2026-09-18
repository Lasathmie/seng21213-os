#include "process.h"
#include "../include/types.h"

pcb_t    process_table[MAX_PROCESSES];
uint32_t current_pid = 0;

/* k_memset — minimal memset for zeroing PCB table (no libc available) */
static void k_memset(void *dst, uint8_t val, uint32_t len)
{
    uint8_t *d = (uint8_t *)dst;
    for (uint32_t i = 0; i < len; i++) d[i] = val;
}

/* k_strncpy — copy at most n bytes, always NUL-terminate */
static void k_strncpy(char *dst, const char *src, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n - 1 && src[i]; i++) dst[i] = src[i];
    dst[i] = '\0';
}

/* process_init — zero the process table and register the kernel itself
 * as PID 0 (the idle / shell process).
 *
 * PID 0 is special: it has no stack allocation here because it IS the
 * current execution context — the stack is already at 0x90000 from the
 * bootloader. We just register it as RUNNING so the scheduler knows
 * it exists and can switch back to it.
 */
void process_init(void)
{
    k_memset(process_table, 0, sizeof(process_table));

    /* PID 0 = kernel/shell — already running, stack set by bootloader */
    process_table[0].pid   = 0;
    process_table[0].state = PROCESS_RUNNING;
    process_table[0].stack = (uint32_t *)0x90000;  /* bootloader stack base */
    k_strncpy(process_table[0].name, "kernel", 32);
    current_pid = 0;
}

/* process_create — allocate a new PCB and set up its initial stack frame
 *
 * The initial stack frame is crafted so context_switch()'s POPAD + RET
 * sequence starts the process correctly:
 *
 * High address (top of stack allocation):
 *   [return addr = entry] <- RET pops this and jumps to entry()
 *   [EAX=0][ECX=0][EDX=0][EBX=0]
 *   [ESP=0][EBP=0][ESI=0][EDI=0]  <- POPAD pops these 8 dwords
 * Low address (ESP saved in PCB points here, to EDI slot)
 *
 * Returns PID on success, -1 if process table is full.
 */
int32_t process_create(const char *name, void (*entry)(void))
{
    /* Find a free slot */
    int32_t slot = -1;
    for (int32_t i = 1; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == PROCESS_FREE) {
            slot = i;
            break;
        }
    }
    if (slot == -1) return -1;   /* table full */

    /* Allocate stack — static pool, one per slot */
    static uint8_t stacks[MAX_PROCESSES][STACK_SIZE];
    uint32_t *stack_top = (uint32_t *)(stacks[slot] + STACK_SIZE);

    /* Build the fake stack frame.
     *
     * context_switch() (switch.asm) always does: pushad / save / load /
     * popad / ret. So the frame this function must build is: a fake
     * return address on top of 8 zeroed registers — when context_switch
     * pops this process in, POPAD consumes the 8 zeros, then RET pops
     * the fake return address and jumps straight into entry(), exactly
     * as if entry() had just been "called".
     */
    *(--stack_top) = (uint32_t)entry;      /* fake return address -> entry() */

    /* Fake PUSHAD frame (8 registers, all zero), in pushad's own order */
    *(--stack_top) = 0;   /* EAX */
    *(--stack_top) = 0;   /* ECX */
    *(--stack_top) = 0;   /* EDX */
    *(--stack_top) = 0;   /* EBX */
    *(--stack_top) = 0;   /* ESP (POPAD ignores this field) */
    *(--stack_top) = 0;   /* EBP */
    *(--stack_top) = 0;   /* ESI */
    *(--stack_top) = 0;   /* EDI */

    /* Fill PCB */
    process_table[slot].pid   = (uint32_t)slot;
    process_table[slot].state = PROCESS_READY;
    process_table[slot].esp   = (uint32_t)stack_top;
    process_table[slot].stack = (uint32_t *)(stacks[slot]);
    k_strncpy(process_table[slot].name, name, 32);

    return slot;
}

/* process_terminate — mark a process as terminated
 * The scheduler will skip TERMINATED slots automatically.
 * Stack memory is reclaimed when the slot is reused.
 */
void process_terminate(uint32_t pid)
{
    if (pid == 0 || pid >= MAX_PROCESSES) return;  /* can't kill kernel */
    process_table[pid].state = PROCESS_TERMINATED;
}
