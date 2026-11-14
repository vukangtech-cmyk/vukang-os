/* VUKANG OS - kernel/mm/paging.c : paging + per-process address spaces
 *
 * Virtual memory layout (every address space):
 *   0x00000000 - kernel_limit   kernel identity map, SUPERVISOR ONLY (no PG_USER)
 *                               page 0 is unmapped to catch NULL dereferences
 *   0x40000000 - 0x7FFFFFFF     user space, private to each process (PG_USER)
 *   0x7FFFB000 - 0x7FFFEFFF     user stack (4 pages), 0x7FFFF000 = guard page
 *   0xC0000000 - ...            framebuffer window (if it lies outside the
 *                               identity map), SUPERVISOR ONLY
 *
 * The kernel page tables are built once and shared by every page directory,
 * so kernel memory is identical everywhere but unreachable from ring 3.
 */
#include "mm.h"
#include "string.h"
#include "kernel.h"
#include "io.h"
#include "fb.h"

#define PD_INDEX(va) ((va) >> 22)
#define PT_INDEX(va) (((va) >> 12) & 0x3FF)
#define FRAME(e)     ((e) & ~0xFFFu)

static uint32_t *kernel_pd;
static uint32_t kernel_tables, kernel_limit;

void paging_init(void) {
    uint32_t limit = pmm_limit();                 /* all managed RAM, <= 1 GiB */
    kernel_tables = (limit + (4u << 20) - 1) >> 22;
    if (kernel_tables > PD_INDEX(USER_BASE)) PANIC("kernel identity map would overlap user space");
    kernel_limit = kernel_tables << 22;

    kernel_pd = (uint32_t *)pmm_alloc_zero();
    if (!kernel_pd) PANIC("no memory for kernel page directory");
    pmm_lock((uint32_t)kernel_pd);
    for (uint32_t t = 0; t < kernel_tables; t++) {
        uint32_t *pt = (uint32_t *)pmm_alloc();
        if (!pt) PANIC("no memory for kernel page tables");
        pmm_lock((uint32_t)pt);                   /* page tables can never be freed */
        for (uint32_t i = 0; i < 1024; i++)
            pt[i] = ((t * 1024 + i) * PAGE_SIZE) | PG_PRESENT | PG_WRITE;   /* supervisor only */
        kernel_pd[t] = (uint32_t)pt | PG_PRESENT | PG_WRITE;
    }
    ((uint32_t *)FRAME(kernel_pd[0]))[0] = 0;     /* unmap page 0 (NULL guard) */

    /* Framebuffer: identity-mapped already if it sits inside the kernel map,
     * otherwise mapped (supervisor-only) at FB_VIRT. Its page tables are
     * created now so every process directory shares them. */
    if (fb.present) {
        if (fb.phys + fb.size <= kernel_limit) {
            pmm_reserve_range(fb.phys, fb.phys + fb.size);   /* never hand out VRAM as RAM */
        } else {
            uint32_t pa0 = fb.phys & ~0xFFFu, pages = (fb.phys - pa0 + fb.size + 0xFFF) >> 12;
            if (FB_VIRT + (pages << 12) < FB_VIRT) PANIC("framebuffer too large");
            for (uint32_t i = 0; i < pages; i++) {
                uint32_t va = FB_VIRT + (i << 12);
                if (!(kernel_pd[PD_INDEX(va)] & PG_PRESENT)) {
                    uint32_t *pt = (uint32_t *)pmm_alloc_zero();
                    if (!pt) PANIC("no memory for framebuffer page tables");
                    pmm_lock((uint32_t)pt);
                    kernel_pd[PD_INDEX(va)] = (uint32_t)pt | PG_PRESENT | PG_WRITE;
                }
                ((uint32_t *)FRAME(kernel_pd[PD_INDEX(va)]))[PT_INDEX(va)] =
                    (pa0 + (i << 12)) | PG_PRESENT | PG_WRITE | 0x008 /* write-through */;
            }
            fb_set_base(FB_VIRT + (fb.phys - pa0));
        }
    }

    __asm__ volatile("mov %0, %%cr3" :: "r"(kernel_pd));
    uint32_t cr0; __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000u | 0x00010000u;             /* PG + WP (honour read-only pages in ring 0 too) */
    __asm__ volatile("mov %0, %%cr0" :: "r"(cr0));
}
uint32_t paging_mapped_mb(void) { return kernel_limit >> 20; }
uint32_t paging_kernel_limit(void) { return kernel_limit; }
uint32_t vmm_kernel_dir(void) { return (uint32_t)kernel_pd; }
uint32_t vmm_current_dir(void) { uint32_t c; __asm__ volatile("mov %%cr3, %0" : "=r"(c)); return c; }
void vmm_switch(uint32_t pd) { if (vmm_current_dir() != pd) __asm__ volatile("mov %0, %%cr3" :: "r"(pd) : "memory"); }

uint32_t vmm_create_space(void) {
    uint32_t *pd = (uint32_t *)pmm_alloc_zero();
    if (!pd) return 0;
    for (uint32_t i = 0; i < kernel_tables; i++) pd[i] = kernel_pd[i];   /* share kernel tables */
    for (uint32_t i = PD_INDEX(USER_END); i < 1024; i++) pd[i] = kernel_pd[i]; /* kernel high window */
    return (uint32_t)pd;
}

static bool is_user_va(uint32_t va) { return va >= USER_BASE && va < USER_END; }

bool vmm_map(uint32_t pdp, uint32_t va, uint32_t pa, uint32_t flags) {
    uint32_t *pd = (uint32_t *)pdp;
    if ((va | pa) & 0xFFF) return false;
    if ((flags & PG_USER) && !is_user_va(va)) return false;    /* never expose kernel range */
    uint32_t pde = pd[PD_INDEX(va)];
    if (!(pde & PG_PRESENT)) {
        if (PD_INDEX(va) < kernel_tables) return false;        /* kernel tables are fixed */
        uint32_t pt = pmm_alloc_zero();
        if (!pt) return false;
        pde = pd[PD_INDEX(va)] = pt | PG_PRESENT | PG_WRITE | (flags & PG_USER);
    }
    uint32_t *pt = (uint32_t *)FRAME(pde);
    pt[PT_INDEX(va)] = pa | (flags & 0xFFF) | PG_PRESENT;
    if (pdp == vmm_current_dir()) __asm__ volatile("invlpg (%0)" :: "r"(va) : "memory");
    return true;
}

uint32_t vmm_lookup(uint32_t pdp, uint32_t va, uint32_t *flags) {
    uint32_t *pd = (uint32_t *)pdp;
    uint32_t pde = pd[PD_INDEX(va)];
    if (!(pde & PG_PRESENT)) return 0;
    uint32_t pte = ((uint32_t *)FRAME(pde))[PT_INDEX(va)];
    if (!(pte & PG_PRESENT)) return 0;
    if (flags) *flags = (pte & pde & (PG_USER | PG_WRITE)) | PG_PRESENT;   /* effective rights */
    return FRAME(pte) | (va & 0xFFF);
}

void vmm_destroy_space(uint32_t pdp) {
    if (!pdp || pdp == (uint32_t)kernel_pd) return;
    if (vmm_current_dir() == pdp) vmm_switch((uint32_t)kernel_pd);
    uint32_t *pd = (uint32_t *)pdp;
    for (uint32_t i = PD_INDEX(USER_BASE); i < PD_INDEX(USER_END); i++) {
        if (!(pd[i] & PG_PRESENT)) continue;
        uint32_t *pt = (uint32_t *)FRAME(pd[i]);
        for (int k = 0; k < 1024; k++) if (pt[k] & PG_PRESENT) pmm_free(FRAME(pt[k]));
        pmm_free((uint32_t)pt);
    }
    pmm_free(pdp);
}

uint32_t vmm_user_pages(uint32_t pdp) {
    uint32_t *pd = (uint32_t *)pdp, n = 0;
    for (uint32_t i = PD_INDEX(USER_BASE); i < PD_INDEX(USER_END); i++)
        if (pd[i] & PG_PRESENT) { uint32_t *pt = (uint32_t *)FRAME(pd[i]); for (int k = 0; k < 1024; k++) n += pt[k] & 1; }
    return n;
}

/* Validate that [va, va+len) is user memory the CURRENT process may access. */
bool vmm_user_range_ok(uint32_t va, uint32_t len, bool write) {
    if (len == 0) return true;
    if (va < USER_BASE || va >= USER_END || len > USER_END - va) return false;   /* overflow-safe */
    uint32_t need = PG_USER | (write ? PG_WRITE : 0), pd = vmm_current_dir();
    for (uint32_t p = va & ~0xFFFu; p < va + len; p += PAGE_SIZE) {
        uint32_t fl;
        if (!vmm_lookup(pd, p, &fl) || (fl & need) != need) return false;
        if (p + PAGE_SIZE < p) break;
    }
    return true;
}
int vmm_user_strlen(uint32_t va, uint32_t max) {
    for (uint32_t i = 0; i < max; i++) {
        if (i == 0 || ((va + i) & 0xFFF) == 0)
            if (!vmm_user_range_ok(va + i, 1, false)) return -1;
        if (*(const char *)(va + i) == 0) return (int)i;
    }
    return -1;
}
