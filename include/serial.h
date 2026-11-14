#ifndef VK_SERIAL_H
#define VK_SERIAL_H
void serial_init(void);
void serial_putc(char c);
int  serial_received(void);
char serial_getc(void);
#endif
