/* VUKANG OS - kernel/mm/heap.c : kernel heap (first-fit, block split + coalescing)
 * Each block has a header with a magic value to detect corruption. */
#include "mm.h"
#include "string.h"
#include "kernel.h"
#include "io.h"

#define HEAP_SIZE  (8u << 20)          /* 8 MiB kernel heap */
#define MAGIC      0x564B4850          /* "VKHP" */
typedef struct block { uint32_t magic, size, free; struct block *next; } block_t;
static block_t *heap_head;
static uint32_t heap_base;

void heap_init(void) {
    /* Find HEAP_SIZE of contiguous free frames above the kernel. */
    extern uint8_t kernel_end[];
    heap_base = ((uint32_t)kernel_end + 0x100000) & ~0xFFFFF;   /* next MiB boundary + 1 MiB */
    pmm_reserve_range(heap_base, heap_base + HEAP_SIZE);
    heap_head = (block_t *)heap_base;
    heap_head->magic = MAGIC; heap_head->size = HEAP_SIZE - sizeof(block_t);
    heap_head->free = 1; heap_head->next = 0;
}

void *kmalloc(size_t n) {
    if (!n) return 0;
    n = (n + 15) & ~15u;
    uint32_t f = irq_save();
    for (block_t *b = heap_head; b; b = b->next) {
        if (b->magic != MAGIC) PANIC("heap corruption detected");
        if (b->free && b->size >= n) {
            if (b->size >= n + sizeof(block_t) + 16) {           /* split */
                block_t *nb = (block_t *)((uint8_t *)(b + 1) + n);
                nb->magic = MAGIC; nb->size = b->size - n - sizeof(block_t);
                nb->free = 1; nb->next = b->next;
                b->next = nb; b->size = n;
            }
            b->free = 0;
            irq_restore(f);
            return b + 1;
        }
    }
    irq_restore(f);
    return 0;
}
void *kzalloc(size_t n) { void *p = kmalloc(n); if (p) memset(p, 0, n); return p; }
void kfree(void *p) {
    if (!p) return;
    uint32_t f = irq_save();
    block_t *b = (block_t *)p - 1;
    if (b->magic != MAGIC || b->free) PANIC("invalid kfree / double free");
    b->free = 1;
    for (block_t *c = heap_head; c; c = c->next)                 /* coalesce neighbours */
        while (c->free && c->next && c->next->free && (uint8_t *)(c + 1) + c->size == (uint8_t *)c->next) {
            c->size += sizeof(block_t) + c->next->size;
            c->next = c->next->next;
        }
    irq_restore(f);
}
void *krealloc(void *p, size_t n) {
    if (!p) return kmalloc(n);
    block_t *b = (block_t *)p - 1;
    if (b->size >= n) return p;
    void *q = kmalloc(n);
    if (q) { memcpy(q, p, b->size); kfree(p); }
    return q;
}
void heap_stats(uint32_t *total, uint32_t *used, uint32_t *blocks) {
    uint32_t t = 0, u = 0, c = 0;
    for (block_t *b = heap_head; b; b = b->next) { t += b->size; if (!b->free) u += b->size; c++; }
    *total = t; *used = u; *blocks = c;
}
