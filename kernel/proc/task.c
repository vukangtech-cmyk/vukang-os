/* VUKANG OS - kernel/proc/task.c : preemptive round-robin multitasking
 *
 * How it works: every timer tick (100 Hz) the interrupt stub saves all CPU
 * registers on the current task's stack and calls schedule(). schedule()
 * stores that stack pointer, picks the next READY task and returns its saved
 * stack pointer. The stub then pops that task's registers and IRETs into it.
 */
#include "task.h"
#include "mm.h"
#include "string.h"
#include "kernel.h"
#include "console.h"
#include "io.h"

#define STACK_SIZE 16384
static task_t tasks[MAX_TASKS];
static int current;
static int next_pid = 1;

void tasking_init(void) {
    memset(tasks, 0, sizeof tasks);
    tasks[0].pid = 0;
    strcpy(tasks[0].name, "kernel/shell");
    tasks[0].state = T_RUNNING;
    current = 0;
}

static void reap(void) {
    for (int i = 1; i < MAX_TASKS; i++)
        if (tasks[i].state == T_DEAD && i != current) {
            kfree(tasks[i].stack);
            tasks[i].stack = 0;
            tasks[i].state = T_UNUSED;
        }
}

int task_create(const char *name, void (*fn)(void *), void *arg) {
    uint32_t f = irq_save();
    reap();
    int slot = -1;
    for (int i = 1; i < MAX_TASKS; i++) if (tasks[i].state == T_UNUSED) { slot = i; break; }
    if (slot < 0) { irq_restore(f); return -1; }
    task_t *t = &tasks[slot];
    t->stack = kmalloc(STACK_SIZE);
    if (!t->stack) { irq_restore(f); return -1; }
    uint32_t *sp = (uint32_t *)(t->stack + STACK_SIZE);
    *--sp = (uint32_t)arg;                 /* argument for fn */
    *--sp = (uint32_t)task_exit;           /* fn returns here */
    *--sp = 0x202;                         /* eflags (IF=1) */
    *--sp = KERNEL_CS;                     /* cs */
    *--sp = (uint32_t)fn;                  /* eip */
    *--sp = 0; *--sp = 0;                  /* err_code, int_no */
    for (int i = 0; i < 8; i++) *--sp = 0; /* pusha block */
    for (int i = 0; i < 4; i++) *--sp = KERNEL_DS;  /* ds es fs gs */
    t->esp = (uint32_t)sp;
    t->pid = next_pid++;
    strncpy(t->name, name, sizeof t->name - 1);
    t->cpu_ticks = 0;
    t->state = T_READY;
    irq_restore(f);
    return t->pid;
}

extern uint32_t timer_ticks(void);
uint32_t schedule(regs_t *r) {
    task_t *cur = &tasks[current];
    cur->esp = (uint32_t)r;
    cur->cpu_ticks++;
    if (cur->state == T_RUNNING) cur->state = T_READY;
    uint32_t now = timer_ticks();
    for (int n = 1; n <= MAX_TASKS; n++) {
        int i = (current + n) % MAX_TASKS;
        task_t *t = &tasks[i];
        if (t->state == T_SLEEPING && (int32_t)(now - t->wake_tick) >= 0) t->state = T_READY;
        if (t->state == T_READY) { current = i; t->state = T_RUNNING; return t->esp; }
    }
    /* nobody else ready: keep running current (task 0 is never dead) */
    if (cur->state == T_DEAD || cur->state == T_SLEEPING) { current = 0; tasks[0].state = T_RUNNING; return tasks[0].esp; }
    cur->state = T_RUNNING;
    return cur->esp;
}

void task_yield(void) { __asm__ volatile("int $0x80" :: "a"(3) : "memory"); }
void task_sleep(uint32_t ms) {
    uint32_t f = irq_save();
    tasks[current].wake_tick = timer_ticks() + (ms * 100) / 1000 + 1;
    tasks[current].state = T_SLEEPING;
    irq_restore(f);
    task_yield();
}
void task_exit(void) {
    __asm__ volatile("cli");
    if (current == 0) PANIC("kernel task tried to exit");
    tasks[current].state = T_DEAD;
    __asm__ volatile("sti");
    for (;;) task_yield();
}
int task_kill(int pid) {
    if (pid == 0) return -2;
    uint32_t f = irq_save();
    for (int i = 1; i < MAX_TASKS; i++)
        if (tasks[i].state != T_UNUSED && tasks[i].state != T_DEAD && tasks[i].pid == pid) {
            tasks[i].state = T_DEAD; irq_restore(f); return 0;
        }
    irq_restore(f);
    return -1;
}
int task_getpid(void) { return tasks[current].pid; }
task_t *task_table(void) { return tasks; }

/* Allow sleeping tasks woken by schedule() */
