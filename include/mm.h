#ifndef VK_MM_H
#define VK_MM_H
#include <stdint.h>
#include <stddef.h>
#define PAGE_SIZE 4096
/* Physical memory manager (bitmap) */
void     pmm_init(uint32_t mb_magic, void *mb_info);
uint32_t pmm_alloc(void);
void     pmm_free(uint32_t addr);
void     pmm_reserve_range(uint32_t start, uint32_t end);
uint32_t pmm_total_pages(void);
uint32_t pmm_used_pages(void);
uint32_t pmm_mem_kb(void);
/* Paging */
void     paging_init(void);
uint32_t paging_mapped_mb(void);
/* Kernel heap */
void     heap_init(void);
void    *kmalloc(size_t n);
void    *kzalloc(size_t n);
void    *krealloc(void *p, size_t n);
void     kfree(void *p);
void     heap_stats(uint32_t *total, uint32_t *used, uint32_t *blocks);
#endif
