/* VUKANG OS - kernel/proc/syscall.c : system calls (int 0x80) + ring-3 user mode
 *
 *   EAX = syscall number, EBX/ECX = arguments, return value in EAX
 *   0 exit(code)   1 write(str)   2 ticks()   3 yield()   4 getpid()   5 sleep(ms)
 */
#include "task.h"
#include "arch.h"
#include "console.h"
#include "kernel.h"
#include "string.h"
#include "io.h"

enum { SYS_EXIT, SYS_WRITE, SYS_TICKS, SYS_YIELD, SYS_GETPID, SYS_SLEEP, SYS_MAX };
static uint32_t syscall_count[SYS_MAX];
int usermode_active;
extern void usermode_return(int code) __attribute__((noreturn));
extern int enter_usermode(void (*entry)(void), uint32_t user_stack);

static uint8_t kstack0[8192] __attribute__((aligned(16)));    /* ring0 stack used on ring3 -> ring0 */
static uint8_t ustack[8192] __attribute__((aligned(16)));     /* user program stack */

void syscall_init(void) { tss_set_kernel_stack((uint32_t)(kstack0 + sizeof kstack0)); }

uint32_t syscall_dispatch(regs_t *r) {
    if (r->eax < SYS_MAX) syscall_count[r->eax]++;
    switch (r->eax) {
    case SYS_EXIT:
        if (usermode_active && (r->cs & 3) == 3) usermode_return((int)r->ebx);
        task_exit(); break;
    case SYS_WRITE: {
        const char *s = (const char *)r->ebx;
        for (int i = 0; s && s[i] && i < 1024; i++) console_putc(s[i]);
        r->eax = 0; break; }
    case SYS_TICKS:  r->eax = timer_ticks(); break;
    case SYS_YIELD:  return schedule(r);
    case SYS_GETPID: r->eax = task_getpid(); break;
    case SYS_SLEEP: {   /* busy-yield inside user mode keeps it simple and safe */
        uint32_t end = timer_ticks() + r->ebx / 10 + 1;
        __asm__ volatile("sti");
        while (timer_ticks() < end) hlt();
        r->eax = 0; break; }
    default: r->eax = (uint32_t)-1;
    }
    return (uint32_t)r;
}

/* ----- user-space demo programs: they may ONLY use int 0x80 ----- */
static inline int u_syscall(int n, uint32_t a) { int r; __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "b"(a) : "memory"); return r; }

static const char msg_hello[] = "  [ring3] Habari! I am a user-mode program running in RING 3.\n";
static const char msg_ticks[] = "  [ring3] I asked the kernel for the time via syscall 2.\n";
static const char msg_try[]   = "  [ring3] Now I will try a privileged instruction (cli)...\n";
static const char msg_bye[]   = "  [ring3] Kwaheri! Exiting with syscall 0.\n";

static void user_program_hello(void) {
    u_syscall(SYS_WRITE, (uint32_t)msg_hello);
    u_syscall(SYS_TICKS, 0);
    u_syscall(SYS_WRITE, (uint32_t)msg_ticks);
    u_syscall(SYS_SLEEP, 200);
    u_syscall(SYS_WRITE, (uint32_t)msg_bye);
    u_syscall(SYS_EXIT, 42);
    for (;;);
}
static void user_program_evil(void) {
    u_syscall(SYS_WRITE, (uint32_t)msg_try);
    __asm__ volatile("cli");          /* General Protection Fault: kernel must survive */
    u_syscall(SYS_EXIT, 0);
    for (;;);
}

static int run(void (*prog)(void)) {
    usermode_active = 1;
    int code = enter_usermode(prog, (uint32_t)(ustack + sizeof ustack));
    usermode_active = 0;
    __asm__ volatile("sti");
    return code;
}
int usermode_run_demo(void) {
    kprintf("Switching CPU to ring 3 (user mode)...\n");
    int c = run(user_program_hello);
    kprintf("Back in kernel (ring 0). Program exit code = %d\n\n", c);
    kprintf("Running an untrusted program that breaks the rules...\n");
    c = run(user_program_evil);
    kprintf("Kernel survived. Program exit code = %d (memory protection works)\n", c);
    return 0;
}
