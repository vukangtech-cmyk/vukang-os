/* VUKANG OS user-space mini library: everything goes through int 0x80 */
#ifndef VK_ULIB_H
#define VK_ULIB_H
#include <stdint.h>
enum { SYS_EXIT, SYS_WRITE, SYS_TICKS, SYS_YIELD, SYS_GETPID, SYS_SLEEP };
static inline int syscall1(int n, uint32_t a) {
    int r; __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "b"(a) : "memory"); return r;
}
static inline void sys_exit(int c) { syscall1(SYS_EXIT, (uint32_t)c); for (;;); }
static inline int  sys_write(const char *s) { return syscall1(SYS_WRITE, (uint32_t)s); }
static inline int  sys_ticks(void) { return syscall1(SYS_TICKS, 0); }
static inline int  sys_getpid(void) { return syscall1(SYS_GETPID, 0); }
static inline void sys_sleep(int ms) { syscall1(SYS_SLEEP, (uint32_t)ms); }
static inline void print(const char *s) { sys_write(s); }
static inline void print_int(int v) {
    char b[12]; int i = 11; unsigned u = v < 0 ? -(unsigned)v : (unsigned)v; b[i] = 0;
    do { b[--i] = '0' + u % 10; u /= 10; } while (u);
    if (v < 0) b[--i] = '-';
    sys_write(b + i);
}
static inline void print_hex(uint32_t v) {
    char b[11] = "0x00000000";
    for (int i = 0; i < 8; i++) b[9 - i] = "0123456789abcdef"[(v >> (i * 4)) & 15];
    sys_write(b);
}
int main(void);
#endif
