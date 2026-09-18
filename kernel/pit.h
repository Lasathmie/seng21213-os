/* =============================================================================
 * SENG21213-OS :: PIT (Programmable Interval Timer) driver
 * File   : kernel/pit.h
 * ============================================================================= */
#ifndef PIT_H
#define PIT_H

#include <types.h>

/* PIT base clock frequency (Hz) */
#define PIT_BASE_HZ   1193182

/* Our desired tick rate */
#define PIT_TARGET_HZ 100

/* Divisor for target frequency */
#define PIT_DIVISOR   (PIT_BASE_HZ / PIT_TARGET_HZ)

/* PIT I/O ports */
#define PIT_CMD       0x43
#define PIT_CHANNEL0  0x40

/* Total ticks since boot — read by scheduler, shell (uptime), etc. */
extern volatile uint32_t tick_count;

void pit_init(void);

#endif
