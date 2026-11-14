/* VUKANG OS - kernel/mm/pmm.c : physical page frame allocator
 *
 * Two bitmaps, one bit per 4 KiB frame:
 *   used[]   - 1 = frame allocated or unusable
 *   locked[] - 1 = frame is permanently owned by the kernel (BIOS area,
 *              kernel image, boot data, heap, kernel page tables). Locked
 *              frames can never be released through pmm_free().
 * pmm_free() validates alignment, bounds, ownership and double-frees
 * before touching any accounting, so a buggy caller cannot corrupt it.
 */
#include "mm.h"
#include "multiboot.h"
#include "string.h"
#include "kernel.h"
#include "console.h"
#include "io.h"

#define MAX_MEM   0x40000000u                 /* 1 GiB: must stay <= USER_BASE */
#define MAX_PAGES (MAX_MEM / PAGE_SIZE)
static uint32_t used[MAX_PAGES / 32];
static uint32_t locked[MAX_PAGES / 32];
static uint32_t total_pages, free_pages, mem_kb, next_hint, rejected;
extern uint8_t kernel_start[], kernel_end[];

#define BIT(map, p)   ((map)[(p) >> 5] & (1u << ((p) & 31)))
#define SET(map, p)   ((map)[(p) >> 5] |= (1u << ((p) & 31)))
#define CLR(map, p)   ((map)[(p) >> 5] &= ~(1u << ((p) & 31)))

static void mark_free(uint64_t start, uint64_t end) {
    if (end > MAX_MEM) end = MAX_MEM;
    if (start >= end) return;
    uint32_t first = (uint32_t)((start + PAGE_SIZE - 1) / PAGE_SIZE);
    uint32_t last  = (uint32_t)(end / PAGE_SIZE);            /* exclusive */
    for (uint32_t p = first; p < last; p++) {
        if (BIT(used, p)) { CLR(used, p); free_pages++; }
        if (p + 1 > total_pages) total_pages = p + 1;
    }
}

void pmm_reserve_range(uint32_t start, uint32_t end) {
    if (end <= start) return;
    uint32_t first = start / PAGE_SIZE;
    uint32_t last  = (end - 1) / PAGE_SIZE;
    for (uint32_t p = first; p <= last && p < MAX_PAGES; p++) {
        if (!BIT(used, p)) { SET(used, p); if (p < total_pages) free_pages--; }
        SET(locked, p);
    }
}

void pmm_init(uint32_t magic, void *info) {
    if (magic != MULTIBOOT_MAGIC) PANIC("not booted by a multiboot loader");
    multiboot_info_t *mb = info;
    memset(used, 0xFF, sizeof used);              /* everything unusable by default */
    memset(locked, 0, sizeof locked);
    total_pages = free_pages = mem_kb = 0;

    if (mb->flags & (1 << 6)) {
        for (uint32_t p = mb->mmap_addr; p + sizeof(multiboot_mmap_t) <= mb->mmap_addr + mb->mmap_length; ) {
            multiboot_mmap_t *e = (multiboot_mmap_t *)p;
            if (e->type == 1) {
                uint64_t base = ((uint64_t)e->addr_hi << 32) | e->addr_lo;
                uint64_t len  = ((uint64_t)e->len_hi << 32) | e->len_lo;
                mark_free(base, base + len);
                if (base < MAX_MEM) mem_kb += (uint32_t)((base + len > MAX_MEM ? MAX_MEM - base : len) / 1024);
            }
            if (e->size == 0) break;               /* malformed entry: stop */
            p += e->size + 4;
        }
    } else if (mb->flags & 1) {
        mark_free(0x100000, 0x100000 + (uint64_t)mb->mem_upper * 1024);
        mem_kb = mb->mem_upper + 1024;
    } else PANIC("bootloader gave no memory information");

    /* Lock everything the kernel depends on BEFORE any allocation happens. */
    pmm_reserve_range(0, 0x100000);                                     /* IVT, BDA, EBDA, VGA, BIOS */
    pmm_reserve_range((uint32_t)kernel_start, (uint32_t)kernel_end);    /* kernel image + bss */
    pmm_reserve_range((uint32_t)mb, (uint32_t)mb + sizeof *mb);         /* boot data */
    if (mb->flags & (1 << 6)) pmm_reserve_range(mb->mmap_addr, mb->mmap_addr + mb->mmap_length);
    if (mb->flags & (1 << 2)) pmm_reserve_range(mb->cmdline, mb->cmdline + 4096);
    if ((mb->flags & (1 << 3)) && mb->mods_count) {
        uint32_t *m = (uint32_t *)mb->mods_addr;
        pmm_reserve_range(mb->mods_addr, mb->mods_addr + mb->mods_count * 16);
        for (uint32_t i = 0; i < mb->mods_count; i++) pmm_reserve_range(m[i * 4], m[i * 4 + 1]);
    }
    if (total_pages * PAGE_SIZE < (16u << 20)) PANIC("VUKANG OS needs at least 16 MiB of RAM");
    next_hint = 0x100000 / PAGE_SIZE;
}

uint32_t pmm_alloc(void) {
    uint32_t f = irq_save();
    for (uint32_t n = 0; n < total_pages; n++) {
        uint32_t p = (next_hint + n) % total_pages;
        if (!BIT(used, p)) {
            SET(used, p); free_pages--; next_hint = p + 1;
            irq_restore(f);
            return p * PAGE_SIZE;
        }
    }
    irq_restore(f);
    return 0;
}
uint32_t pmm_alloc_zero(void) {
    uint32_t a = pmm_alloc();
    if (a) memset((void *)a, 0, PAGE_SIZE);     /* frames are identity-mapped */
    return a;
}

uint32_t pmm_alloc_contiguous(uint32_t pages, uint32_t min_addr, bool lock) {
    if (!pages) return 0;
    uint32_t f = irq_save();
    uint32_t run = 0, start = (min_addr + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint32_t p = start; p < total_pages; p++) {
        run = BIT(used, p) ? 0 : run + 1;
        if (run == pages) {
            uint32_t first = p - pages + 1;
            for (uint32_t k = first; k <= p; k++) { SET(used, k); free_pages--; if (lock) SET(locked, k); }
            irq_restore(f);
            return first * PAGE_SIZE;
        }
    }
    irq_restore(f);
    return 0;
}

void pmm_lock(uint32_t addr) { uint32_t p = addr / PAGE_SIZE; if (p < MAX_PAGES) SET(locked, p); }

pmm_status_t pmm_free(uint32_t addr) {
    pmm_status_t st = PMM_OK;
    uint32_t p = addr / PAGE_SIZE;
    uint32_t f = irq_save();
    if (addr & (PAGE_SIZE - 1))  st = PMM_ERR_UNALIGNED;
    else if (p >= total_pages)   st = PMM_ERR_RANGE;
    else if (BIT(locked, p))     st = PMM_ERR_LOCKED;
    else if (!BIT(used, p))      st = PMM_ERR_DOUBLE_FREE;
    else { CLR(used, p); free_pages++; if (p < next_hint) next_hint = p; }
    if (st != PMM_OK) rejected++;
    irq_restore(f);
    return st;
}

bool pmm_is_free(uint32_t addr) { uint32_t p = addr / PAGE_SIZE; return p < total_pages && !BIT(used, p); }
uint32_t pmm_total_pages(void) { return total_pages; }
uint32_t pmm_free_pages(void) { return free_pages; }
uint32_t pmm_used_pages(void) { return total_pages - free_pages; }
uint32_t pmm_mem_kb(void) { return mem_kb; }
uint32_t pmm_limit(void) { return total_pages * PAGE_SIZE; }
uint32_t pmm_rejected_frees(void) { return rejected; }
const char *pmm_status_str(pmm_status_t s) {
    static const char *n[] = { "ok", "unaligned address", "outside managed RAM", "kernel-owned frame", "double free" };
    return s <= PMM_ERR_DOUBLE_FREE ? n[s] : "?";
}
