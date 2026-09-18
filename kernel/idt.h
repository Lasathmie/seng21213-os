#ifndef IDT_H
#define IDT_H

#include <types.h>

/* One 8-byte IDT gate descriptor */
typedef struct {
    uint16_t offset_low;   /* handler address bits  0-15  */
    uint16_t selector;     /* GDT code segment selector   */
    uint8_t  zero;         /* always 0                    */
    uint8_t  type_attr;    /* present | DPL | gate type   */
    uint16_t offset_high;  /* handler address bits 16-31  */
} __attribute__((packed)) idt_entry_t;

/* Pointer struct passed to LIDT */
typedef struct {
    uint16_t limit;        /* sizeof IDT - 1              */
    uint32_t base;         /* linear address of IDT       */
} __attribute__((packed)) idt_ptr_t;

void idt_set_gate(uint8_t num, uint32_t handler, uint16_t sel, uint8_t flags);
void idt_init(void);

#endif