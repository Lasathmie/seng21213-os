#include "pmm.h"

#define FRAME_SIZE   4096
#define RAM_BYTES    (32 * 1024 * 1024)   /* matches Makefile's `-m 32M` */
#define BITMAP_SIZE  (RAM_BYTES / FRAME_SIZE / 32)

/* Marks the first byte after the kernel image (set in linker.ld).
 * We never hand out a frame at or below this — that's the kernel itself. */
extern uint32_t _kernel_end;
#define KERNEL_END ((uint32_t)&_kernel_end)

static uint32_t bitmap[BITMAP_SIZE];   /* 1 bit per frame */
static uint32_t total_frames;
static uint32_t free_frames;

static inline void bitmap_set  (uint32_t f) { bitmap[f/32] |=  (1u << (f%32)); }
static inline void bitmap_clear(uint32_t f) { bitmap[f/32] &= ~(1u << (f%32)); }
static inline int  bitmap_test (uint32_t f) { return (bitmap[f/32] >> (f%32)) & 1; }

/* k_memset — no libc in a freestanding kernel */
static void k_memset(void *dst, uint8_t val, uint32_t len)
{
    uint8_t *d = (uint8_t *)dst;
    for (uint32_t i = 0; i < len; i++) d[i] = val;
}

void pmm_init(void)
{
    /* Start with every frame marked used; we only clear (free) the
     * ones the E820 map says are usable RAM above the kernel. */
    k_memset(bitmap, 0xFF, sizeof(bitmap));
    free_frames = 0;

    uint16_t      count = *(uint16_t *)0x8000;
    e820_entry_t *map   = (e820_entry_t *)0x8004;

    for (int i = 0; i < count; i++) {
        if (map[i].type != 1) continue;   /* type 1 = usable RAM */

        uint32_t start = (uint32_t)map[i].base;
        uint32_t len   = (uint32_t)map[i].length;
        uint32_t end   = start + len;

        /* Skip the first 1 MB (BIOS/legacy devices) and anything the
         * kernel image itself occupies. */
        uint32_t first = (start < KERNEL_END) ? KERNEL_END : start;

        for (uint32_t a = first; a + FRAME_SIZE <= end; a += FRAME_SIZE) {
            uint32_t frame = a / FRAME_SIZE;
            if (frame >= (RAM_BYTES / FRAME_SIZE)) break; /* stay in-bitmap */
            if (a >= 0x00200000 && a < 0x00300000) continue; /* RAM disk */
            if (bitmap_test(frame)) {
                bitmap_clear(frame);
                free_frames++;
            }
        }
    }

    if (count == 0 || count > 64) {
        for (uint32_t a = 0x100000; a + FRAME_SIZE <= RAM_BYTES; a += FRAME_SIZE) {
            if (a >= 0x00200000 && a < 0x00300000) continue; /* RAM disk */
            uint32_t frame = a / FRAME_SIZE;
            if (bitmap_test(frame)) {
                bitmap_clear(frame);
                free_frames++;
            }
        }
    }

    total_frames = RAM_BYTES / FRAME_SIZE;
}

uint32_t pmm_alloc_frame(void)
{
    for (uint32_t i = 0; i < total_frames; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            free_frames--;
            return i * FRAME_SIZE;   /* physical address */
        }
    }
    return 0;   /* out of memory */
}

void pmm_free_frame(uint32_t phys_addr)
{
    uint32_t frame = phys_addr / FRAME_SIZE;
    if (frame >= total_frames) return;
    if (bitmap_test(frame)) {
        bitmap_clear(frame);
        free_frames++;
    }
}

uint32_t pmm_free_frames(void)  { return free_frames; }
uint32_t pmm_total_frames(void) { return total_frames; }