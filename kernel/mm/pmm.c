/* VUKANG OS - kernel/mm/pmm.c : physical page frame allocator (bitmap, 4 KiB frames)
 * Reads the BIOS memory map from GRUB (multiboot) and marks usable RAM free. */
#include "mm.h"
#include "multiboot.h"
#include "string.h"
#include "kernel.h"
#include "io.h"

#define MAX_MEM   (1024u * 1024u * 1024u)      /* manage up to 1 GiB */
#define MAX_PAGES (MAX_MEM / PAGE_SIZE)
static uint32_t bitmap[MAX_PAGES / 32];
static uint32_t total_pages, used_pages, mem_kb, last_free;
extern uint8_t kernel_start[], kernel_end[];

static inline void set_bit(uint32_t p)   { bitmap[p >> 5] |=  (1u << (p & 31)); }
static inline void clear_bit(uint32_t p) { bitmap[p >> 5] &= ~(1u << (p & 31)); }
static inline int  test_bit(uint32_t p)  { return bitmap[p >> 5] & (1u << (p & 31)); }

static void mark_free(uint32_t start, uint32_t end) {
    start = (start + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    for (uint32_t a = start; a + PAGE_SIZE <= end && a < MAX_MEM; a += PAGE_SIZE) {
        uint32_t p = a / PAGE_SIZE;
        if (test_bit(p)) { clear_bit(p); used_pages--; }
        if (p + 1 > total_pages) total_pages = p + 1;
    }
}
void pmm_reserve_range(uint32_t start, uint32_t end) {
    for (uint32_t a = start & ~(PAGE_SIZE - 1); a < end && a < MAX_MEM; a += PAGE_SIZE) {
        uint32_t p = a / PAGE_SIZE;
        if (!test_bit(p)) { set_bit(p); used_pages++; }
    }
}

void pmm_init(uint32_t magic, void *info) {
    memset(bitmap, 0xFF, sizeof bitmap);         /* everything reserved by default */
    used_pages = MAX_PAGES;
    multiboot_info_t *mb = info;
    if (magic != MULTIBOOT_MAGIC) PANIC("not booted by a multiboot loader");
    if (mb->flags & (1 << 6)) {
        uint32_t p = mb->mmap_addr;
        while (p < mb->mmap_addr + mb->mmap_length) {
            multiboot_mmap_t *e = (multiboot_mmap_t *)p;
            if (e->type == 1 && e->addr_hi == 0) {
                uint32_t end = (e->len_hi || e->addr_lo + e->len_lo < e->addr_lo) ? 0xFFFFFFFF : e->addr_lo + e->len_lo;
                mark_free(e->addr_lo, end);
                mem_kb += e->len_lo / 1024;
            }
            p += e->size + 4;
        }
    } else {
        mark_free(0x100000, 0x100000 + mb->mem_upper * 1024);
        mem_kb = mb->mem_upper + 1024;
    }
    pmm_reserve_range(0, 0x100000);                                  /* BIOS, VGA, real-mode area */
    pmm_reserve_range((uint32_t)kernel_start, (uint32_t)kernel_end); /* the kernel image itself */
    last_free = 0x100000 / PAGE_SIZE;
}

uint32_t pmm_alloc(void) {
    uint32_t f = irq_save();
    for (uint32_t n = 0; n < total_pages; n++) {
        uint32_t p = (last_free + n) % total_pages;
        if (!test_bit(p)) { set_bit(p); used_pages++; last_free = p + 1; irq_restore(f); return p * PAGE_SIZE; }
    }
    irq_restore(f);
    return 0;
}
void pmm_free(uint32_t addr) {
    uint32_t f = irq_save();
    uint32_t p = addr / PAGE_SIZE;
    if (test_bit(p)) { clear_bit(p); used_pages--; }
    irq_restore(f);
}
uint32_t pmm_total_pages(void) { return total_pages; }
uint32_t pmm_used_pages(void) { return used_pages - (MAX_PAGES - total_pages); }
uint32_t pmm_mem_kb(void) { return mem_kb; }
