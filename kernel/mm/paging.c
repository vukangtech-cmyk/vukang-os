/* VUKANG OS - kernel/mm/paging.c : x86 two-level paging (4 KiB pages)
 * Identity-maps physical RAM so virtual == physical for the kernel. */
#include "mm.h"
#include "string.h"
#include "kernel.h"

#define PG_PRESENT 0x1
#define PG_WRITE   0x2
#define PG_USER    0x4    /* demo user programs live in kernel image, so allow ring 3 */

static uint32_t page_dir[1024] __attribute__((aligned(4096)));
static uint32_t mapped_mb;

void paging_init(void) {
    uint32_t mem = pmm_total_pages() * PAGE_SIZE;
    if (mem < 16u << 20) mem = 16u << 20;
    uint32_t tables = (mem + (4u << 20) - 1) / (4u << 20);   /* each table maps 4 MiB */
    if (tables > 256) tables = 256;
    memset(page_dir, 0, sizeof page_dir);
    for (uint32_t t = 0; t < tables; t++) {
        uint32_t *pt = (uint32_t *)pmm_alloc();
        if (!pt) PANIC("out of memory building page tables");
        for (int i = 0; i < 1024; i++)
            pt[i] = ((t * 1024 + i) * PAGE_SIZE) | PG_PRESENT | PG_WRITE | PG_USER;
        page_dir[t] = (uint32_t)pt | PG_PRESENT | PG_WRITE | PG_USER;
    }
    /* Leave page 0 unmapped so NULL-pointer bugs trigger a page fault. */
    ((uint32_t *)(page_dir[0] & ~0xFFF))[0] = 0;
    mapped_mb = tables * 4;
    __asm__ volatile("mov %0, %%cr3" :: "r"(page_dir));
    uint32_t cr0; __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;                                        /* PG bit */
    __asm__ volatile("mov %0, %%cr0" :: "r"(cr0));
}
uint32_t paging_mapped_mb(void) { return mapped_mb; }
