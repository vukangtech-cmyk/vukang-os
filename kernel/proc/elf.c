/* VUKANG OS - kernel/proc/elf.c : ELF32 loader for user programs
 * Every field coming from the file is bounds-checked before use, and all
 * segments must lie entirely inside user space [USER_BASE, USER_END). */
#include "task.h"
#include "mm.h"
#include "string.h"

typedef struct {
    uint8_t  ident[16];
    uint16_t type, machine;
    uint32_t version, entry, phoff, shoff, flags;
    uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
} __attribute__((packed)) elf_hdr_t;
typedef struct { uint32_t type, offset, vaddr, paddr, filesz, memsz, flags, align; } __attribute__((packed)) elf_phdr_t;
#define PT_LOAD 1
#define PF_W    2

int elf_load(uint32_t pd, const uint8_t *img, uint32_t size, uint32_t *entry) {
    if (size < sizeof(elf_hdr_t)) return -1;
    const elf_hdr_t *h = (const elf_hdr_t *)img;
    if (memcmp(h->ident, "\x7F" "ELF", 4) || h->ident[4] != 1 /* 32-bit */ || h->ident[5] != 1 /* LE */) return -1;
    if (h->type != 2 /* EXEC */ || h->machine != 3 /* i386 */) return -1;
    if (h->phentsize != sizeof(elf_phdr_t) || h->phnum == 0 || h->phnum > 16) return -1;
    if (h->phoff > size || (uint32_t)h->phnum * sizeof(elf_phdr_t) > size - h->phoff) return -1;
    if (h->entry < USER_BASE || h->entry >= USER_END) return -1;

    const elf_phdr_t *ph = (const elf_phdr_t *)(img + h->phoff);
    int loaded = 0;
    for (int i = 0; i < h->phnum; i++) {
        const elf_phdr_t *p = &ph[i];
        if (p->type != PT_LOAD || p->memsz == 0) continue;
        if (p->filesz > p->memsz) return -1;
        if (p->offset > size || p->filesz > size - p->offset) return -1;
        if (p->vaddr < USER_BASE || p->memsz > USER_END - p->vaddr) return -1;
        if (p->vaddr + p->memsz > USER_STACK_TOP - USER_STACK_PAGES * PAGE_SIZE) return -1;
        uint32_t flags = PG_USER | ((p->flags & PF_W) ? PG_WRITE : 0);
        for (uint32_t va = p->vaddr & ~0xFFFu; va < p->vaddr + p->memsz; va += PAGE_SIZE) {
            uint32_t fl, pa = vmm_lookup(pd, va, &fl);
            if (!pa) {
                pa = pmm_alloc_zero();
                if (!pa || !vmm_map(pd, va, pa, flags)) { if (pa) pmm_free(pa); return -2; }
            } else if (flags & PG_WRITE) vmm_map(pd, va, pa & ~0xFFFu, flags);
            /* copy the part of the file that lands in this page (through the identity map) */
            uint32_t pstart = va, pend = va + PAGE_SIZE;
            uint32_t fstart = p->vaddr, fend = p->vaddr + p->filesz;
            uint32_t s = pstart > fstart ? pstart : fstart, e = pend < fend ? pend : fend;
            if (s < e) memcpy((void *)((pa & ~0xFFFu) + (s - va)), img + p->offset + (s - p->vaddr), e - s);
        }
        loaded++;
    }
    if (!loaded) return -1;
    *entry = h->entry;
    return 0;
}
