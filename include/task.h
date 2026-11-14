#ifndef VK_TASK_H
#define VK_TASK_H
#include <stdint.h>
#include <stdbool.h>
#include "arch.h"
#define MAX_TASKS 32
#define KSTACK_SIZE 16384
typedef enum { T_UNUSED = 0, T_READY, T_RUNNING, T_SLEEPING, T_DEAD } task_state_t;

/* exit codes for processes killed by the kernel */
#define EXIT_PAGEFAULT  (-11)
#define EXIT_GPF        (-13)
#define EXIT_FAULT      (-4)
#define EXIT_KILLED     (-9)

typedef struct task {
    int          pid;
    char         name[24];
    task_state_t state;
    bool         user;          /* ring-3 process */
    bool         idle;
    uint32_t     esp;           /* saved kernel stack pointer */
    uint8_t     *stack;         /* kernel stack (kmalloc) */
    uint32_t     pd;            /* page directory (physical), kernel dir for kernel tasks */
    uint32_t     wake_tick;
    uint32_t     cpu_ticks;
    int          exit_code;
    int          parent;
} task_t;

void     tasking_init(void);
int      task_create(const char *name, void (*fn)(void *), void *arg);
int      process_spawn(const char *prog);      /* load embedded ELF, returns pid or <0 */
int      task_wait(int pid);
int      task_try_wait(int pid, int *code);                   /* blocks until pid exits, returns exit code */
void     task_detach(int pid);                 /* no one will wait: auto-reap */
void     task_exit(void);
void     task_mark_dead(int code);             /* current task; caller must reschedule */
int      task_kill(int pid);
void     task_yield(void);
void     task_sleep(uint32_t ms);
void     task_sleep_current(uint32_t ms);      /* mark only, caller reschedules */
int      task_getpid(void);
task_t  *task_current(void);
uint32_t schedule(regs_t *r);
task_t  *task_table(void);

/* ELF + embedded user programs */
typedef struct { const char *name; const uint8_t *data; uint32_t size; } user_prog_t;
const user_prog_t *user_prog_find(const char *name);
const user_prog_t *user_prog_list(void);
int      elf_load(uint32_t pd, const uint8_t *img, uint32_t size, uint32_t *entry);
/* Syscalls */
void     syscall_init(void);
uint32_t syscall_count(int n);
#endif
