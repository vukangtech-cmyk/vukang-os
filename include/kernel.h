#ifndef VK_KERNEL_H
#define VK_KERNEL_H
#define VUKANG_NAME    "VUKANG OS"
#define VUKANG_VERSION "0.1.0"
#define VUKANG_CODENAME "Kilimanjaro"
void panic(const char *msg, const char *file, int line) __attribute__((noreturn));
#define PANIC(m) panic(m, __FILE__, __LINE__)
#define ASSERT(c) do { if (!(c)) panic("ASSERT: " #c, __FILE__, __LINE__); } while (0)
void shell_run(void);
void klog(const char *tag, const char *fmt, ...);
#endif
