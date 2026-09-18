/* =============================================================================
 * SENG21213-OS :: Interrupt Descriptor Table
 * File   : kernel/idt.c
 *
 *DEBUG COUNT: 3
 * ============================================================================= */
#include "idt.h"

/* 256 entries — full IDT */
static idt_entry_t idt[256];
static idt_ptr_t   idt_ptr;

/* idt_set_gate — fill one IDT entry
 *   num     : interrupt vector number (0-255)
 *   handler : physical address of the ISR function
 *   sel     : GDT code segment selector (always 0x08 in our flat model)
 *   flags   : type+attr byte (0x8E = present, ring0, 32-bit interrupt gate)
 */
void idt_set_gate(uint8_t num, uint32_t handler, uint16_t sel, uint8_t flags)
{
    idt[num].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt[num].selector    = sel;
    idt[num].zero        = 0;
    idt[num].type_attr   = flags;
    idt[num].offset_high = (uint16_t)((handler >> 16) & 0xFFFF);
}

/* idt_init — zero the IDT, set the pointer, load it with LIDT
 * We register no handlers here — callers (pic, pit) register their own.
 * The IDT starts all-zero (not present), so unregistered vectors
 * that somehow fire will cause a General Protection Fault, which
 * is easier to debug than silent corruption.
 */

/* irq0_handler is defined in kernel/isr_stub.asm */
extern void irq0_handler(void);
extern void irq1_handler(void);

void idt_init(void)
{
    idt_ptr.limit = (uint16_t)(sizeof(idt_entry_t) * 256 - 1);
    idt_ptr.base  = (uint32_t)&idt;

    /* Zero all entries — marks them all as not-present */
    uint8_t *p = (uint8_t *)idt;
    for (uint32_t i = 0; i < sizeof(idt); i++) p[i] = 0;

    /* Vector 32 = IRQ0 (timer) after PIC remap
     * 0x08 = kernel code segment selector
     * 0x8E = present | ring0 | 32-bit interrupt gate */
    idt_set_gate(32, (uint32_t)irq0_handler, 0x08, 0x8E);
    idt_set_gate(33, (uint32_t)irq1_handler, 0x08, 0x8E);

    /* Load the IDT register */
    __asm__ volatile("lidt %0" : : "m"(idt_ptr));
}
