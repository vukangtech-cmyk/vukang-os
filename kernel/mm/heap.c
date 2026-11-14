/* VUKANG OS - kernel/mm/heap.c : hardened kernel heap
 *
 * - The heap region is obtained from the PMM as one contiguous, LOCKED run
 *   of frames, so it can never overlap the kernel, boot data, page tables
 *   or anything else, and it is verified to be inside the identity map.
 * - kmalloc() rejects sizes that would overflow when rounded/padded.
 * - Every block: header {magic, size, req, free, next} + a 4-byte canary
 *   right after the requested size to catch buffer overruns.
 * - kfree() validates the pointer BEFORE reading the header: it must be
 *   inside the heap, correctly aligned and be the start of a real block.
 * - Freed memory is poisoned (0xDD) to expose use-after-free bugs.
 */
#include "mm.h"
#include "string.h"
#include "kernel.h"
#include "console.h"
#include "io.h"

#define HEAP_MAX    (8u << 20)
#define HEAP_MIN    (1u << 20)
#define MAGIC_USED  0x564B4855u     /* "VKHU" */
#define MAGIC_FREE  0x564B4846u     /* "VKHF" */
#define CANARY      0xC0FFEE42u
#define ALIGN       16u

typedef struct block {
    uint32_t magic;
    uint32_t size;          /* usable bytes after header */
    uint32_t req;           /* bytes requested by caller */
    struct block *next;
} block_t;                  /* 16 bytes: keeps payload 16-byte aligned */

static block_t *head;
static uint32_t heap_start, heap_end, rejected;

void heap_init(void) {
    extern uint8_t kernel_end[];
    uint32_t size = HEAP_MAX, base = 0;
    while (size >= HEAP_MIN && !(base = pmm_alloc_contiguous(size / PAGE_SIZE, (uint32_t)kernel_end, true)))
        size /= 2;
    if (!base) PANIC("not enough contiguous RAM for the kernel heap");
    if (base < (uint32_t)kernel_end) PANIC("heap overlaps the kernel image");
    if (base + size > paging_kernel_limit()) PANIC("heap is not fully identity-mapped");
    heap_start = base; heap_end = base + size;
    head = (block_t *)base;
    head->magic = MAGIC_FREE; head->size = size - sizeof(block_t); head->req = 0; head->next = 0;
}

static inline uint32_t *canary_of(block_t *b) { return (uint32_t *)((uint8_t *)(b + 1) + b->req); }

void *kmalloc(size_t n) {
    if (n == 0 || n > heap_end - heap_start) return 0;                /* also blocks n+pad overflow */
    uint32_t need = ((uint32_t)n + sizeof(uint32_t) + ALIGN - 1) & ~(ALIGN - 1);
    uint32_t f = irq_save();
    for (block_t *b = head; b; b = b->next) {
        if (b->magic != MAGIC_USED && b->magic != MAGIC_FREE) PANIC("heap corruption: bad block header");
        if (b->magic != MAGIC_FREE || b->size < need) continue;
        if (b->size >= need + sizeof(block_t) + ALIGN) {               /* split */
            block_t *nb = (block_t *)((uint8_t *)(b + 1) + need);
            nb->magic = MAGIC_FREE; nb->size = b->size - need - sizeof(block_t);
            nb->req = 0; nb->next = b->next;
            b->next = nb; b->size = need;
        }
        b->magic = MAGIC_USED; b->req = n;
        *canary_of(b) = CANARY;
        irq_restore(f);
        return b + 1;
    }
    irq_restore(f);
    return 0;
}
void *kzalloc(size_t n) { void *p = kmalloc(n); if (p) memset(p, 0, n); return p; }

/* Is p the payload address of a live block? Validates before dereferencing. */
static block_t *validate(const void *p) {
    uint32_t a = (uint32_t)p;
    if (a < heap_start + sizeof(block_t) || a >= heap_end || (a - heap_start) % ALIGN) return 0;
    for (block_t *b = head; b; b = b->next)        /* must be a real block start */
        if ((uint32_t)(b + 1) == a) return b;
        else if ((uint32_t)(b + 1) > a) break;
    return 0;
}
bool heap_owns(const void *p) { return validate(p) != 0; }

void kfree(void *p) {
    if (!p) return;
    uint32_t f = irq_save();
    block_t *b = validate(p);
    if (!b) { rejected++; irq_restore(f); kprintf("[heap] rejected kfree(%p): not a heap block\n", p); return; }
    if (b->magic == MAGIC_FREE) { rejected++; irq_restore(f); kprintf("[heap] rejected kfree(%p): double free\n", p); return; }
    if (*canary_of(b) != CANARY) PANIC("heap buffer overflow detected (canary smashed)");
    memset(b + 1, 0xDD, b->size);                  /* poison */
    b->magic = MAGIC_FREE; b->req = 0;
    for (block_t *c = head; c; c = c->next)        /* coalesce adjacent free blocks */
        while (c->magic == MAGIC_FREE && c->next && c->next->magic == MAGIC_FREE &&
               (uint8_t *)(c + 1) + c->size == (uint8_t *)c->next) {
            c->size += sizeof(block_t) + c->next->size;
            c->next = c->next->next;
        }
    irq_restore(f);
}

void *krealloc(void *p, size_t n) {
    if (!p) return kmalloc(n);
    if (!n) { kfree(p); return 0; }
    block_t *b = validate(p);
    if (!b || b->magic != MAGIC_USED) return 0;
    if (n + sizeof(uint32_t) <= b->size && n < b->size) { b->req = n; *canary_of(b) = CANARY; return p; }
    void *q = kmalloc(n);
    if (q) { memcpy(q, p, b->req < n ? b->req : n); kfree(p); }
    return q;
}

int heap_check(void) {
    int errors = 0; uint32_t f = irq_save();
    for (block_t *b = head; b; b = b->next) {
        if ((uint32_t)b < heap_start || (uint32_t)(b + 1) + b->size > heap_end) { errors++; break; }
        if (b->magic != MAGIC_USED && b->magic != MAGIC_FREE) { errors++; break; }
        if (b->magic == MAGIC_USED && *canary_of(b) != CANARY) errors++;
        if (b->next && (uint32_t)b->next <= (uint32_t)b) { errors++; break; }
    }
    irq_restore(f);
    return errors;
}
void heap_stats(uint32_t *total, uint32_t *used, uint32_t *blocks) {
    uint32_t t = 0, u = 0, c = 0; uint32_t f = irq_save();
    for (block_t *b = head; b; b = b->next) { t += b->size; if (b->magic == MAGIC_USED) u += b->size; c++; }
    irq_restore(f);
    *total = t; *used = u; *blocks = c;
}
void heap_range(uint32_t *s, uint32_t *e) { *s = heap_start; *e = heap_end; }
uint32_t heap_rejected_frees(void) { return rejected; }
