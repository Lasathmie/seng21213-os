/* =============================================================================
 * SENG21213-OS :: PIT driver
 * File   : kernel/pit.c
 * ============================================================================= */
#include "pit.h"

/* tick_count incremented by irq0_handler every timer tick */
volatile uint32_t tick_count = 0;

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* pit_init — program PIT channel 0 to fire IRQ0 at PIT_TARGET_HZ
 *
 * Command byte 0x36 decoded:
 *   bits 7-6 = 00  -> select channel 0
 *   bits 5-4 = 11  -> lo/hi byte access mode
 *   bits 3-1 = 011 -> mode 3 (square wave)
 *   bit  0   = 0   -> binary counting
 *
 * We send the divisor low byte first, then high byte, to port 0x40.
 */
void pit_init(void)
{
    uint16_t divisor = (uint16_t)PIT_DIVISOR;

    outb(PIT_CMD,      0x36);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));
}
