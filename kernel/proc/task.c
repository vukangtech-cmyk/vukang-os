/* VUKANG OS - kernel/proc/task.c : preemptive multitasking + user processes
 *
 * Every timer tick the interrupt stub saves all registers on the current
 * task's kernel stack and calls schedule(). schedule() stores that stack
 * pointer, picks the next READY task, switches the page directory (CR3)
 * and the TSS ring-0 stack if needed, and returns the new task's stack
 * pointer; the stub pops its registers and IRETs into it.
 *
 * An idle task (hlt loop) always exists, so sleeping tasks really sleep.
 */
#include "task.h"
#include "mm.h"
#include "string.h"
#include "kernel.h"
#include "console.h"
#include "io.h"

static task_t tasks[MAX_TASKS];
static int current;
static int next_pid = 1;

static void idle_fn(void *a) { (void)a; for (;;) __asm__ volatile("sti; hlt"); }

void tasking_init(void) {
    memset(tasks, 0, sizeof tasks);
    tasks[0].pid = 0;
    strcpy(tasks[0].name, "kernel/shell");
    tasks[0].state = T_RUNNING;
    tasks[0].pd = vmm_kernel_dir();
    current = 0;
    int pid = task_create("idle", idle_fn, 0);
    for (int i = 0; i < MAX_TASKS; i++) if (tasks[i].pid == pid) tasks[i].idle = true;
}

static void free_task(task_t *t) {          /* caller holds irq lock, t != current */
    if (t->user) vmm_destroy_space(t->pd);
    kfree(t->stack);
    memset(t, 0, sizeof *t);
}
static void reap(void) {
    for (int i = 1; i < MAX_TASKS; i++)
        if (tasks[i].state == T_DEAD && i != current && tasks[i].parent < 0) free_task(&tasks[i]);
}

static task_t *alloc_slot(const char *name) {
    reap();
    for (int i = 1; i < MAX_TASKS; i++)
        if (tasks[i].state == T_UNUSED) {
            task_t *t = &tasks[i];
            memset(t, 0, sizeof *t);
            if (!(t->stack = kmalloc(KSTACK_SIZE))) return 0;
            t->pid = next_pid++;
            strncpy(t->name, name, sizeof t->name - 1);
            t->pd = vmm_kernel_dir();
            t->parent = -1;
            return t;
        }
    return 0;
}

/* Build the interrupt frame the task will "return" into. */
static uint32_t *push_frame(task_t *t, uint32_t *sp, uint32_t eip, uint32_t cs, uint32_t ds) {
    *--sp = 0x202;                         /* eflags: IF=1 */
    *--sp = cs;
    *--sp = eip;
    *--sp = 0; *--sp = 0;                  /* err_code, int_no */
    for (int i = 0; i < 8; i++) *--sp = 0; /* pusha block */
    for (int i = 0; i < 4; i++) *--sp = ds;
    t->esp = (uint32_t)sp;
    return sp;
}

int task_create(const char *name, void (*fn)(void *), void *arg) {
    uint32_t f = irq_save();
    task_t *t = alloc_slot(name);
    if (!t) { irq_restore(f); return -1; }
    uint32_t *sp = (uint32_t *)(t->stack + KSTACK_SIZE);
    *--sp = (uint32_t)arg;
    *--sp = (uint32_t)task_exit;           /* fn returns here */
    push_frame(t, sp, (uint32_t)fn, KERNEL_CS, KERNEL_DS);
    t->state = T_READY;
    int pid = t->pid;
    irq_restore(f);
    return pid;
}

int process_spawn(const char *progname) {
    const user_prog_t *prog = user_prog_find(progname);
    if (!prog) return -1;
    uint32_t pd = vmm_create_space();
    if (!pd) return -2;
    uint32_t entry;
    int r = elf_load(pd, prog->data, prog->size, &entry);
    if (r < 0) { vmm_destroy_space(pd); return -3; }
    for (uint32_t i = 1; i <= USER_STACK_PAGES; i++) {             /* stack, guard page above stays unmapped */
        uint32_t fr = pmm_alloc_zero();
        if (!fr || !vmm_map(pd, USER_STACK_TOP - i * PAGE_SIZE, fr, PG_USER | PG_WRITE)) {
            if (fr) pmm_free(fr);
            vmm_destroy_space(pd); return -2;
        }
    }
    uint32_t f = irq_save();
    task_t *t = alloc_slot(prog->name);
    if (!t) { irq_restore(f); vmm_destroy_space(pd); return -4; }
    t->user = true; t->pd = pd; t->parent = tasks[current].pid;
    uint32_t *sp = (uint32_t *)(t->stack + KSTACK_SIZE);
    *--sp = USER_DS;                       /* ss  (ring 3) */
    *--sp = USER_STACK_TOP - 16;           /* esp (ring 3) */
    push_frame(t, sp, entry, USER_CS, USER_DS);
    t->state = T_READY;
    int pid = t->pid;
    irq_restore(f);
    return pid;
}

/* 1 = finished (code stored, task freed), 0 = still running, -1 = no such pid */
int task_try_wait(int pid, int *code) {
    uint32_t f = irq_save();
    task_t *t = 0;
    for (int i = 1; i < MAX_TASKS; i++) if (tasks[i].state != T_UNUSED && tasks[i].pid == pid) t = &tasks[i];
    if (!t) { irq_restore(f); return -1; }
    if (t->state == T_DEAD) { if (code) *code = t->exit_code; free_task(t); irq_restore(f); return 1; }
    irq_restore(f);
    return 0;
}
int task_wait(int pid) {
    int code, r;
    while ((r = task_try_wait(pid, &code)) == 0) task_sleep(20);
    return r < 0 ? -1 : code;
}

extern uint32_t timer_ticks(void);
uint32_t schedule(regs_t *r) {
    task_t *cur = &tasks[current];
    cur->esp = (uint32_t)r;
    cur->cpu_ticks++;
    if (cur->state == T_RUNNING) cur->state = T_READY;
    uint32_t now = timer_ticks();
    int pick = -1, idle = -1;
    for (int n = 1; n <= MAX_TASKS; n++) {
        int i = (current + n) % MAX_TASKS;
        task_t *t = &tasks[i];
        if (t->state == T_SLEEPING && (int32_t)(now - t->wake_tick) >= 0) t->state = T_READY;
        if (t->state != T_READY) continue;
        if (t->idle) { idle = i; continue; }
        pick = i; break;
    }
    if (pick < 0) pick = idle >= 0 ? idle : 0;
    current = pick;
    task_t *t = &tasks[current];
    t->state = T_RUNNING;
    if (t->stack) tss_set_kernel_stack((uint32_t)(t->stack + KSTACK_SIZE));
    vmm_switch(t->pd);
    return t->esp;
}

void task_yield(void) { __asm__ volatile("int $0x80" :: "a"(3) : "memory"); }
void task_sleep_current(uint32_t ms) {
    uint32_t ticks = (ms * timer_hz() + 999) / 1000;
    tasks[current].wake_tick = timer_ticks() + (ticks ? ticks : 1);
    tasks[current].state = T_SLEEPING;
}
void task_sleep(uint32_t ms) {
    uint32_t f = irq_save();
    task_sleep_current(ms);
    irq_restore(f);
    task_yield();
}
void task_mark_dead(int code) {
    if (current == 0) PANIC("kernel shell task tried to exit");
    tasks[current].exit_code = code;
    tasks[current].state = T_DEAD;
}
void task_exit(void) {
    __asm__ volatile("cli");
    task_mark_dead(0);
    __asm__ volatile("sti");
    for (;;) task_yield();
}
int task_kill(int pid) {
    if (pid == 0) return -2;
    uint32_t f = irq_save();
    for (int i = 1; i < MAX_TASKS; i++)
        if (tasks[i].state != T_UNUSED && tasks[i].state != T_DEAD && tasks[i].pid == pid) {
            if (tasks[i].idle) { irq_restore(f); return -2; }
            tasks[i].exit_code = EXIT_KILLED;
            tasks[i].state = T_DEAD;
            irq_restore(f); return 0;
        }
    irq_restore(f);
    return -1;
}
void task_detach(int pid) {
    uint32_t f = irq_save();
    for (int i = 1; i < MAX_TASKS; i++) if (tasks[i].state != T_UNUSED && tasks[i].pid == pid) tasks[i].parent = -1;
    irq_restore(f);
}
int task_getpid(void) { return tasks[current].pid; }
task_t *task_current(void) { return &tasks[current]; }
task_t *task_table(void) { return tasks; }
