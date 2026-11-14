/* VUKANG OS - kernel/arch/gdt.c : Global Descriptor Table + Task State Segment
 * Segments: 0 null | 1 kernel code | 2 kernel data | 3 user code | 4 user data | 5 TSS */
#include "arch.h"
#include "string.h"

typedef struct { uint16_t limit_lo, base_lo; uint8_t base_mid, access, gran, base_hi; } __attribute__((packed)) gdt_entry_t;
typedef struct { uint16_t limit; uint32_t base; } __attribute__((packed)) gdt_ptr_t;
typedef struct {
    uint32_t prev, esp0, ss0, esp1, ss1, esp2, ss2, cr3, eip, eflags;
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs, ldt;
    uint16_t trap, iomap;
} __attribute__((packed)) tss_t;

static gdt_entry_t gdt[6];
static gdt_ptr_t gp;
static tss_t tss;
extern void gdt_flush(uint32_t);
extern void tss_flush(void);

static void set(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[i].base_lo = base & 0xFFFF; gdt[i].base_mid = (base >> 16) & 0xFF; gdt[i].base_hi = base >> 24;
    gdt[i].limit_lo = limit & 0xFFFF;
    gdt[i].gran = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[i].access = access;
}
void tss_set_kernel_stack(uint32_t esp0) { tss.esp0 = esp0; }

void gdt_init(void) {
    gp.limit = sizeof(gdt) - 1; gp.base = (uint32_t)&gdt;
    set(0, 0, 0, 0, 0);
    set(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);   /* ring0 code */
    set(2, 0, 0xFFFFFFFF, 0x92, 0xCF);   /* ring0 data */
    set(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);   /* ring3 code */
    set(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);   /* ring3 data */
    memset(&tss, 0, sizeof tss);
    tss.ss0 = KERNEL_DS; tss.iomap = sizeof tss;
    set(5, (uint32_t)&tss, sizeof tss - 1, 0x89, 0x00);
    gdt_flush((uint32_t)&gp);
    tss_flush();
}
