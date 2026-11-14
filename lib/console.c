/* VUKANG OS - lib/console.c : kprintf + unified VGA/serial console */
#include <stdarg.h>
#include <stdint.h>
#include "console.h"
#include "vga.h"
#include "serial.h"
#include "drivers.h"
#include "string.h"
#include "io.h"

void console_putc(char c) {
    uint32_t f = irq_save();
    vga_putc(c);
    if (c == '\n') serial_putc('\r');
    serial_putc(c);
    irq_restore(f);
}
void console_write(const char *s) { while (*s) console_putc(*s++); }

/* ---- minimal vsnprintf: %d %i %u %x %X %p %s %c %% with width, '-', '0' ---- */
typedef struct { char *buf; size_t n, pos; } out_t;
static void emit(out_t *o, char c) { if (o->pos + 1 < o->n) o->buf[o->pos] = c; o->pos++; }

static void emit_num(out_t *o, uint32_t v, int base, int neg, int width, int zero, int left, int upper) {
    char tmp[16]; int i = 0;
    const char *dig = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    do { tmp[i++] = dig[v % base]; v /= base; } while (v);
    int len = i + (neg ? 1 : 0);
    if (!left && !zero) while (width-- > len) emit(o, ' ');
    if (neg) emit(o, '-');
    if (!left && zero) while (width-- > len) emit(o, '0');
    while (i) emit(o, tmp[--i]);
    if (left) while (width-- > len) emit(o, ' ');
}

int kvsnprintf(char *buf, size_t n, const char *fmt, va_list ap) {
    out_t o = { buf, n, 0 };
    for (; *fmt; fmt++) {
        if (*fmt != '%') { emit(&o, *fmt); continue; }
        fmt++;
        int left = 0, zero = 0, width = 0;
        for (;; fmt++) { if (*fmt == '-') left = 1; else if (*fmt == '0') zero = 1; else break; }
        if (*fmt == '*') { width = va_arg(ap, int); if (width < 0) { left = 1; width = -width; } fmt++; }
        while (isdigit(*fmt)) width = width * 10 + (*fmt++ - '0');
        switch (*fmt) {
        case 'd': case 'i': { int v = va_arg(ap, int);
            emit_num(&o, v < 0 ? (uint32_t)-v : (uint32_t)v, 10, v < 0, width, zero, left, 0); break; }
        case 'u': emit_num(&o, va_arg(ap, uint32_t), 10, 0, width, zero, left, 0); break;
        case 'x': emit_num(&o, va_arg(ap, uint32_t), 16, 0, width, zero, left, 0); break;
        case 'X': emit_num(&o, va_arg(ap, uint32_t), 16, 0, width, zero, left, 1); break;
        case 'p': emit(&o, '0'); emit(&o, 'x'); emit_num(&o, va_arg(ap, uint32_t), 16, 0, 8, 1, 0, 0); break;
        case 'c': emit(&o, (char)va_arg(ap, int)); break;
        case 's': { const char *s = va_arg(ap, const char *); if (!s) s = "(null)";
            int len = (int)strlen(s);
            if (!left) while (width-- > len) emit(&o, ' ');
            while (*s) emit(&o, *s++);
            if (left) while (width-- > len) emit(&o, ' ');
            break; }
        case '%': emit(&o, '%'); break;
        case 0: fmt--; break;
        default: emit(&o, '%'); emit(&o, *fmt);
        }
    }
    if (n) buf[o.pos < n ? o.pos : n - 1] = 0;
    return (int)o.pos;
}
int ksnprintf(char *buf, size_t n, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); int r = kvsnprintf(buf, n, fmt, ap); va_end(ap); return r;
}
int kprintf(const char *fmt, ...) {
    char buf[512]; va_list ap;
    va_start(ap, fmt); int r = kvsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    console_write(buf);
    return r;
}

/* Blocking read: from PS/2 keyboard or serial (so the OS works headless too). */
int console_getc(void) {
    for (;;) {
        int c = keyboard_getc_nonblock();
        if (c >= 0) return c;
        if (serial_received()) {
            char s = serial_getc();
            if (s == '\r') s = '\n';
            if (s == 127) s = '\b';
            return s;
        }
        hlt();   /* sleep until next interrupt (timer/keyboard) */
    }
}

int console_readline(char *buf, int max) {
    int n = 0;
    for (;;) {
        int c = console_getc();
        if (c == '\n') { console_putc('\n'); buf[n] = 0; return n; }
        if (c == '\b') { if (n > 0) { n--; console_write("\b \b"); } continue; }
        if (c < 32 || c > 126) continue;
        if (n < max - 1) { buf[n++] = (char)c; console_putc((char)c); }
    }
}
