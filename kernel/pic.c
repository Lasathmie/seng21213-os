/* =============================================================================
 * SENG21213-OS :: 8259 PIC driver
 * File   : kernel/pic.c
 * ============================================================================= */
#include "pic.h"

/* outb — write one byte to an I/O port
 * Defined here as a static inline so pic.c has no dependency on vga.c's outb.
 * (vga.c has its own private outb — that is fine, they don't conflict.)
 */
static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* io_wait — tiny delay after each PIC command byte.
 * Old hardware needs a few microseconds between PIC writes.
 * Writing to port 0x80 (POST code port, unused in normal operation)
 * is the standard delay trick used by every real OS kernel.
 */
static inline void io_wait(void)
{
    __asm__ volatile("outb %%al, $0x80" : : "a"((uint8_t)0));
}

/* pic_init — remap both PICs and mask all IRQs except IRQ0 and IRQ1
 *
 * Remap is mandatory: without it IRQ0 (timer) fires on CPU vector 8
 * which is the double-fault exception handler — instant triple fault.
 *
 * Mask strategy: start with all IRQs masked (0xFF), then selectively
 * unmask only what we need:
 *   IRQ0 (bit 0 of master) = timer    -> needed for scheduler
 *   IRQ1 (bit 1 of master) = keyboard -> needed for shell
 */
void pic_init(void)
{
    /* ICW1: start initialization sequence */
    outb(PIC1_CMD,  PIC_ICW1); io_wait();
    outb(PIC2_CMD,  PIC_ICW1); io_wait();

    /* ICW2: set vector offsets */
    outb(PIC1_DATA, PIC1_OFFSET); io_wait();   /* master: IRQ0 -> vector 32 */
    outb(PIC2_DATA, PIC2_OFFSET); io_wait();   /* slave:  IRQ8 -> vector 40 */

    /* ICW3: tell master slave is on IRQ2, tell slave its cascade identity */
    outb(PIC1_DATA, 0x04); io_wait();   /* master: slave on IRQ2 (bit 2) */
    outb(PIC2_DATA, 0x02); io_wait();   /* slave:  cascade identity = 2   */

    /* ICW4: set 8086 mode */
    outb(PIC1_DATA, PIC_ICW4); io_wait();
    outb(PIC2_DATA, PIC_ICW4); io_wait();

    /* Mask all IRQs first, then unmask only IRQ0 and IRQ1
     * Mask register: bit=1 means MASKED (disabled), bit=0 means UNMASKED
     * 0xFC = 11111100 -> only bits 0 and 1 cleared -> IRQ0 + IRQ1 unmasked
     */
    outb(PIC1_DATA, 0xFC);   /* master: unmask IRQ0 (timer) + IRQ1 (keyboard) */
    outb(PIC2_DATA, 0xFF);   /* slave:  mask everything                        */
}

/* pic_send_eoi — signal End Of Interrupt to the PIC(s)
 * Must be called at the END of every IRQ handler, or the PIC
 * will never fire that IRQ again (it thinks we're still handling it).
 * If the IRQ came from the slave (IRQ >= 8), we must EOI both PICs.
 */
void pic_send_eoi(uint8_t irq)
{
    if (irq >= 8)
        outb(PIC2_CMD, PIC_EOI);   /* slave first if cascade IRQ */
    outb(PIC1_CMD, PIC_EOI);
}
