#ifndef VK_IO_H
#define VK_IO_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile("outb %0,%1" :: "a"(v), "Nd"(p)); }
static inline void outw(uint16_t p, uint16_t v) { __asm__ volatile("outw %0,%1" :: "a"(v), "Nd"(p)); }
static inline uint8_t inb(uint16_t p) { uint8_t v; __asm__ volatile("inb %1,%0" : "=a"(v) : "Nd"(p)); return v; }
static inline uint16_t inw(uint16_t p) { uint16_t v; __asm__ volatile("inw %1,%0" : "=a"(v) : "Nd"(p)); return v; }
static inline void io_wait(void) { outb(0x80, 0); }

/* Save interrupt flag and disable interrupts (critical section). */
static inline uint32_t irq_save(void) {
    uint32_t f; __asm__ volatile("pushf; pop %0; cli" : "=r"(f) :: "memory"); return f;
}
static inline void irq_restore(uint32_t f) { if (f & 0x200) __asm__ volatile("sti" ::: "memory"); }
static inline void hlt(void) { __asm__ volatile("hlt"); }
#endif
