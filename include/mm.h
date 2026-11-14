#ifndef VK_MM_H
#define VK_MM_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#define PAGE_SIZE 4096

/* ---------------- Physical memory manager ---------------- */
typedef enum {
    PMM_OK = 0,
    PMM_ERR_UNALIGNED,     /* address not a multiple of 4096 */
    PMM_ERR_RANGE,         /* frame outside managed RAM */
    PMM_ERR_LOCKED,        /* frame owned by kernel/BIOS/heap/page tables */
    PMM_ERR_DOUBLE_FREE    /* frame already free */
} pmm_status_t;

void         pmm_init(uint32_t mb_magic, void *mb_info);
uint32_t     pmm_alloc(void);                         /* 0 on failure */
uint32_t     pmm_alloc_zero(void);
uint32_t     pmm_alloc_contiguous(uint32_t pages, uint32_t min_addr, bool locked);
pmm_status_t pmm_free(uint32_t addr);
void         pmm_lock(uint32_t addr);                 /* mark frame kernel-owned forever */
void         pmm_reserve_range(uint32_t start, uint32_t end);
bool         pmm_is_free(uint32_t addr);
uint32_t     pmm_total_pages(void);
uint32_t     pmm_used_pages(void);
uint32_t     pmm_free_pages(void);
uint32_t     pmm_mem_kb(void);
uint32_t     pmm_limit(void);                         /* first byte NOT managed */
uint32_t     pmm_rejected_frees(void);
const char  *pmm_status_str(pmm_status_t s);

/* ---------------- Paging / virtual memory ---------------- */
#define PG_PRESENT 0x001
#define PG_WRITE   0x002
#define PG_USER    0x004

#define USER_BASE       0x40000000u   /* user programs live in [1 GiB, 2 GiB) */
#define USER_END        0x80000000u
#define USER_STACK_TOP  0x7FFFF000u   /* last page left unmapped as guard */
#define USER_STACK_PAGES 4

void     paging_init(void);
uint32_t paging_mapped_mb(void);
uint32_t paging_kernel_limit(void);                   /* identity map ends here */
uint32_t vmm_kernel_dir(void);
uint32_t vmm_create_space(void);                      /* new page directory (phys) */
void     vmm_destroy_space(uint32_t pd);
bool     vmm_map(uint32_t pd, uint32_t va, uint32_t pa, uint32_t flags);
uint32_t vmm_lookup(uint32_t pd, uint32_t va, uint32_t *flags);  /* phys or 0 */
uint32_t vmm_current_dir(void);
void     vmm_switch(uint32_t pd);
uint32_t vmm_user_pages(uint32_t pd);
bool     vmm_user_range_ok(uint32_t va, uint32_t len, bool write);
int      vmm_user_strlen(uint32_t va, uint32_t max);  /* -1 if invalid */

/* ---------------- Kernel heap ---------------- */
void    heap_init(void);
void   *kmalloc(size_t n);
void   *kzalloc(size_t n);
void   *krealloc(void *p, size_t n);
void    kfree(void *p);
bool    heap_owns(const void *p);
int     heap_check(void);                             /* 0 = healthy, else #errors */
void    heap_stats(uint32_t *total, uint32_t *used, uint32_t *blocks);
void    heap_range(uint32_t *start, uint32_t *end);
uint32_t heap_rejected_frees(void);
#endif
