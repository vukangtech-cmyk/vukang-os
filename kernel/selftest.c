/* VUKANG OS - kernel/selftest.c : in-kernel tests for the hardened memory code
 * Each test deliberately performs a bad operation and checks that the kernel
 * detects and refuses it without corrupting its own state. */
#include "kernel.h"
#include "mm.h"
#include "console.h"
#include "vga.h"
#include "string.h"
#include "task.h"

static int pass, fail;
static void t(const char *name, int ok) {
    uint8_t c = vga_get_color();
    vga_set_color(ok ? VGA_LGREEN : VGA_LRED, VGA_BLACK);
    kprintf("  [%s] ", ok ? "PASS" : "FAIL");
    vga_set_color(c & 0x0F, c >> 4);
    kprintf("%s\n", name);
    if (ok) pass++; else fail++;
}

int kernel_selftest(void) {
    extern uint8_t kernel_start[];
    pass = fail = 0;
    kprintf("Physical memory manager:\n");
    uint32_t before = pmm_free_pages();
    uint32_t fr = pmm_alloc();
    t("allocate a frame", fr != 0 && (fr & 0xFFF) == 0 && pmm_free_pages() == before - 1);
    t("free it", pmm_free(fr) == PMM_OK && pmm_free_pages() == before);
    t("reject double free", pmm_free(fr) == PMM_ERR_DOUBLE_FREE && pmm_free_pages() == before);
    t("reject unaligned address", pmm_free(fr + 12) == PMM_ERR_UNALIGNED);
    t("reject frame outside RAM", pmm_free(0xFFFFF000) == PMM_ERR_RANGE);
    t("reject freeing kernel image", pmm_free((uint32_t)kernel_start) == PMM_ERR_LOCKED);
    t("reject freeing BIOS area", pmm_free(0x000B8000) == PMM_ERR_LOCKED);
    t("reject freeing page directory", pmm_free(vmm_kernel_dir()) == PMM_ERR_LOCKED);
    uint32_t hs, he; heap_range(&hs, &he);
    t("reject freeing heap frames", pmm_free(hs) == PMM_ERR_LOCKED);
    t("accounting unchanged", pmm_free_pages() == before);

    kprintf("Kernel heap:\n");
    t("reject huge size (overflow)", kmalloc(0xFFFFFFF8u) == 0 && kmalloc(0xFFFFFFFFu) == 0);
    t("reject size larger than heap", kmalloc(he - hs + 1) == 0);
    char *a = kmalloc(100), *b = kmalloc(100);
    t("allocate + 16-byte alignment", a && b && ((uint32_t)a & 15) == 0 && ((uint32_t)b & 15) == 0);
    uint32_t rej = heap_rejected_frees();
    kfree((void *)0x00200123);                         /* pointer outside / not a block */
    kfree(a + 8);                                      /* pointer into the middle of a block */
    t("reject foreign/interior pointers", heap_rejected_frees() == rej + 2);
    kfree(a);
    kfree(a);                                          /* double free */
    t("reject double free", heap_rejected_frees() == rej + 3);
    t("freed memory poisoned (0xDD)", (uint8_t)a[0] == 0xDD);
    kfree(b);
    t("heap integrity check", heap_check() == 0);

    kprintf("Paging:\n");
    uint32_t fl;
    t("kernel pages are supervisor-only", vmm_lookup(vmm_kernel_dir(), 0x00100000, &fl) && !(fl & PG_USER));
    t("NULL page unmapped", vmm_lookup(vmm_kernel_dir(), 0, 0) == 0);
    uint32_t pd = vmm_create_space();
    t("refuse PG_USER on kernel range", pd && !vmm_map(pd, 0x00100000, 0x00100000, PG_USER | PG_WRITE));
    uint32_t f2 = pmm_alloc_zero();
    t("map user page in private space", vmm_map(pd, USER_BASE, f2, PG_USER | PG_WRITE) &&
                                        vmm_lookup(pd, USER_BASE, &fl) && (fl & PG_USER));
    t("user page invisible to kernel dir", vmm_lookup(vmm_kernel_dir(), USER_BASE, 0) == 0);
    uint32_t fp = pmm_free_pages();
    vmm_destroy_space(pd);
    t("destroying space frees its frames", pmm_free_pages() == fp + 3);   /* page, table, directory */

    kprintf("\n%d passed, %d failed\n", pass, fail);
    return fail;
}
