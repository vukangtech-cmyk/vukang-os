#ifndef VK_ARCH_H
#define VK_ARCH_H
#include <stdint.h>

/* Register frame pushed by isr.asm for every interrupt. */
typedef struct regs {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;     /* useresp/ss only on ring change */
} regs_t;

typedef uint32_t (*irq_handler_t)(regs_t *r);  /* returns ESP to resume */

#define KERNEL_CS 0x08
#define KERNEL_DS 0x10
#define USER_CS   0x1B
#define USER_DS   0x23

void gdt_init(void);
void tss_set_kernel_stack(uint32_t esp0);
void idt_init(void);
void idt_set_gate(uint8_t n, uint32_t base, uint16_t sel, uint8_t flags);
void irq_register(int irq, irq_handler_t h);
void pic_remap(void);
void pic_eoi(int irq);
void timer_init(uint32_t hz);
uint32_t timer_ticks(void);
uint32_t timer_hz(void);
void timer_sleep(uint32_t ms);
#endif
