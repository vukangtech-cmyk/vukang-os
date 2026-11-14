/* VUKANG OS - kernel/arch/timer.c : PIT 8253 timer (IRQ0) drives the scheduler */
#include "arch.h"
#include "io.h"
#include "task.h"
static volatile uint32_t ticks;
static uint32_t hz = 100;
static uint32_t timer_irq(regs_t *r) { ticks++; return schedule(r); }
void timer_init(uint32_t f) {
    hz = f;
    uint32_t div = 1193182 / f;
    outb(0x43, 0x36); outb(0x40, div & 0xFF); outb(0x40, (div >> 8) & 0xFF);
    irq_register(0, timer_irq);
}
uint32_t timer_ticks(void) { return ticks; }
uint32_t timer_hz(void) { return hz; }
void timer_sleep(uint32_t ms) {
    uint32_t end = ticks + (ms * hz) / 1000 + 1;
    while (ticks < end) hlt();
}
