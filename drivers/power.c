/* VUKANG OS - drivers/power.c : reboot, shutdown, PC speaker */
#include "drivers.h"
#include "arch.h"
#include "io.h"
void power_reboot(void) {
    __asm__ volatile("cli");
    while (inb(0x64) & 0x02);
    outb(0x64, 0xFE);                     /* 8042 reset line */
    for (;;) hlt();
}
void power_shutdown(void) {
    __asm__ volatile("cli");
    outw(0x604, 0x2000);                  /* QEMU (ACPI PM1a) */
    outw(0xB004, 0x2000);                 /* Bochs / old QEMU */
    outw(0x4004, 0x3400);                 /* VirtualBox */
    for (;;) hlt();
}
void pcspk_beep(uint32_t freq, uint32_t ms) {
    if (!freq) return;
    uint32_t div = 1193180 / freq;
    outb(0x43, 0xB6); outb(0x42, div & 0xFF); outb(0x42, div >> 8);
    outb(0x61, inb(0x61) | 3);
    timer_sleep(ms);
    outb(0x61, inb(0x61) & 0xFC);
}
