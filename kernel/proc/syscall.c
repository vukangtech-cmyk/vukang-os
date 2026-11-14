/* VUKANG OS - kernel/proc/syscall.c : system calls (int 0x80)
 *
 *   EAX = number, EBX/ECX = arguments, result in EAX (negative = error)
 *   0 exit(code)  1 write(str)  2 ticks()  3 yield()  4 getpid()  5 sleep(ms)
 *
 * Every pointer from user space is validated against the CURRENT page
 * directory (present + user-accessible) before the kernel touches it, so a
 * process cannot make the kernel read kernel memory on its behalf.
 */
#include "task.h"
#include "arch.h"
#include "mm.h"
#include "console.h"
#include "kernel.h"
#include "string.h"
#include "io.h"

enum { SYS_EXIT, SYS_WRITE, SYS_TICKS, SYS_YIELD, SYS_GETPID, SYS_SLEEP, SYS_MAX };
#define EFAULT 14
#define ENOSYS 38
static uint32_t counts[SYS_MAX];
uint32_t syscall_count(int n) { return n >= 0 && n < SYS_MAX ? counts[n] : 0; }
void syscall_init(void) { }

uint32_t syscall_dispatch(regs_t *r) {
    bool from_user = (r->cs & 3) == 3;
    if (r->eax < SYS_MAX) counts[r->eax]++;
    switch (r->eax) {
    case SYS_EXIT:
        task_mark_dead((int)r->ebx);
        return schedule(r);
    case SYS_WRITE: {
        int len;
        if (from_user) {
            len = vmm_user_strlen(r->ebx, 4096);
            if (len < 0) { r->eax = (uint32_t)-EFAULT; break; }
        } else len = (int)strlen((const char *)r->ebx);
        const char *s = (const char *)r->ebx;
        for (int i = 0; i < len; i++) console_putc(s[i]);
        r->eax = len; break; }
    case SYS_TICKS:  r->eax = timer_ticks(); break;
    case SYS_YIELD:  return schedule(r);
    case SYS_GETPID: r->eax = task_getpid(); break;
    case SYS_SLEEP:
        r->eax = 0;
        task_sleep_current(r->ebx > 60000 ? 60000 : r->ebx);
        return schedule(r);
    default: r->eax = (uint32_t)-ENOSYS;
    }
    return (uint32_t)r;
}
