#ifndef VK_TASK_H
#define VK_TASK_H
#include <stdint.h>
#include "arch.h"
#define MAX_TASKS 32
typedef enum { T_UNUSED = 0, T_READY, T_RUNNING, T_SLEEPING, T_DEAD } task_state_t;
typedef struct task {
    int          pid;
    char         name[24];
    task_state_t state;
    uint32_t     esp;
    uint8_t     *stack;
    uint32_t     wake_tick;
    uint32_t     cpu_ticks;
} task_t;
void     tasking_init(void);
int      task_create(const char *name, void (*fn)(void *), void *arg);
void     task_exit(void);
int      task_kill(int pid);
void     task_yield(void);
void     task_sleep(uint32_t ms);
int      task_getpid(void);
uint32_t schedule(regs_t *r);
task_t  *task_table(void);
/* Syscalls & user mode */
void     syscall_init(void);
int      usermode_run_demo(void);
#endif
