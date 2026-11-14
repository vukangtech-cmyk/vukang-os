/* VUKANG OS - drivers/serial.c : COM1 UART 16550 (38400 8N1) */
#include "serial.h"
#include "io.h"
#define COM1 0x3F8
void serial_init(void) {
    outb(COM1 + 1, 0x00);  /* disable UART interrupts (we poll) */
    outb(COM1 + 3, 0x80);  /* DLAB on */
    outb(COM1 + 0, 0x03);  /* divisor 3 -> 38400 baud */
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);  /* 8 bits, no parity, 1 stop */
    outb(COM1 + 2, 0xC7);  /* FIFO */
    outb(COM1 + 4, 0x0B);
}
void serial_putc(char c) { while (!(inb(COM1 + 5) & 0x20)); outb(COM1, (uint8_t)c); }
int  serial_received(void) { return inb(COM1 + 5) & 1; }
char serial_getc(void) { while (!serial_received()); return (char)inb(COM1); }
