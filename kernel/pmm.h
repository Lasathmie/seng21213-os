#ifndef PMM_H
#define PMM_H

#include <types.h>

/* One entry as written by boot.asm's E820 loop at 0x8004+.
 * 'acpi_ext' is the extended attributes dword some BIOSes return when
 * asked for a 24-byte entry (ECX=24) — present in the buffer even if
 * we don't use it, so the struct must be 24 bytes to line up right.
 */
typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;       /* 1 = usable RAM, anything else = reserved/other */
    uint32_t acpi_ext;
} __attribute__((packed)) e820_entry_t;

void     pmm_init(void);
uint32_t pmm_alloc_frame(void);   /* returns physical address, 0 = OOM */
void     pmm_free_frame(uint32_t phys_addr);
uint32_t pmm_free_frames(void);
uint32_t pmm_total_frames(void);

#endif