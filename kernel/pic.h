/* =============================================================================
 * SENG21213-OS :: 8259 PIC driver
 * File   : kernel/pic.h
 * ============================================================================= */
#ifndef PIC_H
#define PIC_H

#include <types.h>

/* Master PIC ports */
#define PIC1_CMD    0x20
#define PIC1_DATA   0x21

/* Slave PIC ports */
#define PIC2_CMD    0xA0
#define PIC2_DATA   0xA1

/* Initialization Command Words */
#define PIC_ICW1    0x11   /* init + cascaded + ICW4 needed  */
#define PIC_ICW4    0x01   /* 8086 mode                      */

/* After remap: IRQ0-7 -> vectors 0x20-0x27 */
#define PIC1_OFFSET 0x20
/* After remap: IRQ8-15 -> vectors 0x28-0x2F */
#define PIC2_OFFSET 0x28

/* End-of-interrupt command */
#define PIC_EOI     0x20

void pic_init(void);
void pic_send_eoi(uint8_t irq);

#endif
