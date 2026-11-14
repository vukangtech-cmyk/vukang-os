/* VUKANG OS - kernel/arch/idt.c : IDT, PIC, exception + IRQ dispatch */
#include "arch.h"
#include "io.h"
#include "console.h"
#include "kernel.h"
#include "task.h"
#include "vga.h"
#include "mm.h"

typedef struct { uint16_t base_lo, sel; uint8_t zero, flags; uint16_t base_hi; } __attribute__((packed)) idt_entry_t;
typedef struct { uint16_t limit; uint32_t base; } __attribute__((packed)) idt_ptr_t;
static idt_entry_t idt[256];
static idt_ptr_t ip;
static irq_handler_t irq_handlers[16];
extern uint32_t isr_stub_table[48];
extern void isr128(void);
extern uint32_t syscall_dispatch(regs_t *r);

static const char *exc_names[32] = {
    "Division By Zero", "Debug", "Non Maskable Interrupt", "Breakpoint", "Overflow",
    "Bound Range Exceeded", "Invalid Opcode", "Device Not Available", "Double Fault",
    "Coprocessor Segment Overrun", "Invalid TSS", "Segment Not Present", "Stack-Segment Fault",
    "General Protection Fault", "Page Fault", "Reserved", "x87 FPU Error", "Alignment Check",
    "Machine Check", "SIMD FP Exception", "Virtualization", "Control Protection",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Hypervisor Injection", "VMM Communication", "Security Exception", "Reserved" };

void idt_set_gate(uint8_t n, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[n].base_lo = base & 0xFFFF; idt[n].base_hi = base >> 16;
    idt[n].sel = sel; idt[n].zero = 0; idt[n].flags = flags;
}

void pic_remap(void) {
    uint8_t m1 = inb(0x21), m2 = inb(0xA1);
    outb(0x20, 0x11); io_wait(); outb(0xA0, 0x11); io_wait();
    outb(0x21, 0x20); io_wait(); outb(0xA1, 0x28); io_wait();   /* IRQ0-15 -> int 32-47 */
    outb(0x21, 0x04); io_wait(); outb(0xA1, 0x02); io_wait();
    outb(0x21, 0x01); io_wait(); outb(0xA1, 0x01); io_wait();
    (void)m1; (void)m2;
    outb(0x21, 0x00); outb(0xA1, 0x00);                          /* unmask all */
}
void pic_eoi(int irq) { if (irq >= 8) outb(0xA0, 0x20); outb(0x20, 0x20); }
void irq_register(int irq, irq_handler_t h) { irq_handlers[irq] = h; }

void idt_init(void) {
    ip.limit = sizeof(idt) - 1; ip.base = (uint32_t)&idt;
    for (int i = 0; i < 48; i++) idt_set_gate(i, isr_stub_table[i], KERNEL_CS, 0x8E);
    idt_set_gate(0x80, (uint32_t)isr128, KERNEL_CS, 0xEE);  /* DPL3: callable from user mode */
    pic_remap();
    __asm__ volatile("lidt %0" :: "m"(ip));
}

static uint32_t exception(regs_t *r) {
    uint32_t cr2; __asm__ volatile("mov %%cr2,%0" : "=r"(cr2));
    if ((r->cs & 3) == 3) {            /* fault in a user process: kill only that process */
        task_t *t = task_current();
        uint8_t c = vga_get_color(); vga_set_color(VGA_LRED, VGA_BLACK);
        kprintf("\n[pid %d %s] %s", t->pid, t->name, exc_names[r->int_no]);
        if (r->int_no == 14)
            kprintf(": %s %s address %p (%s)", ((r->err_code & 0x10) || r->eip == cr2) ? "execute at" : (r->err_code & 2) ? "write to" : "read from",
                    cr2 < USER_BASE ? "KERNEL" : "user", cr2,
                    (r->err_code & 1) ? "page is supervisor-only / protected" : "page not mapped");
        kprintf(" at eip=%p -> process killed\n", r->eip);
        vga_set_color(c & 0x0F, c >> 4);
        task_mark_dead(r->int_no == 14 ? EXIT_PAGEFAULT : r->int_no == 13 ? EXIT_GPF : EXIT_FAULT);
        return schedule(r);
    }
    vga_set_color(VGA_WHITE, VGA_RED);
    kprintf("\n\n*** VUKANG OS KERNEL EXCEPTION ***\n");
    kprintf(" %s (int %u, err %x)\n", exc_names[r->int_no], r->int_no, r->err_code);
    kprintf(" EIP=%p CS=%x EFLAGS=%x CR2=%p\n", r->eip, r->cs, r->eflags, cr2);
    kprintf(" EAX=%p EBX=%p ECX=%p EDX=%p\n", r->eax, r->ebx, r->ecx, r->edx);
    kprintf(" ESI=%p EDI=%p EBP=%p\n", r->esi, r->edi, r->ebp);
    PANIC("unhandled CPU exception in kernel mode");
}

uint32_t interrupt_dispatch(regs_t *r) {
    if (r->int_no < 32) return exception(r);
    if (r->int_no == 0x80) return syscall_dispatch(r);
    int irq = r->int_no - 32;
    pic_eoi(irq);
    if (irq_handlers[irq]) return irq_handlers[irq](r);
    return (uint32_t)r;
}
