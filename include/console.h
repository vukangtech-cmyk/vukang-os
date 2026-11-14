#ifndef VK_CONSOLE_H
#define VK_CONSOLE_H
#include <stdarg.h>
#include <stddef.h>
/* Unified console: writes go to VGA text screen AND serial port COM1. */
void console_putc(char c);
void console_write(const char *s);
int  kprintf(const char *fmt, ...);
int  kvsnprintf(char *buf, size_t n, const char *fmt, va_list ap);
int  ksnprintf(char *buf, size_t n, const char *fmt, ...);
int  console_getc(void);                 /* blocking: keyboard or serial */
int  console_readline(char *buf, int max);
#endif
