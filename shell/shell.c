/* =============================================================
 *  VUKANG OS - shell/shell.c : VKSH, the interactive command shell
 * ============================================================= */
#include "kernel.h"
#include "console.h"
#include "string.h"
#include "vga.h"
#include "fb.h"
#include "arch.h"
#include "mm.h"
#include "task.h"
#include "drivers.h"
#include "fs.h"
#include "db.h"
#include "io.h"
#include "serial.h"

#define LINE 256
typedef struct { const char *name, *args, *help; void (*fn)(char *args); } cmd_t;
static const cmd_t *find_cmd(const char *n);
extern const cmd_t commands[];

static char *skip(char *s) { while (*s == ' ') s++; return s; }
static char *word(char **s) {             /* split first word off */
    char *p = skip(*s), *w = p;
    while (*p && *p != ' ') p++;
    if (*p) *p++ = 0;
    *s = p;
    return w;
}
static void color(uint8_t fg) { vga_set_color(fg, VGA_BLACK); }

/* ---------------- system ---------------- */
static void c_help(char *a) {
    (void)a;
    color(VGA_YELLOW); kprintf("VUKANG OS shell commands:\n"); color(VGA_LGREY);
    for (const cmd_t *c = commands; c->name; c++) {
        char head[32]; ksnprintf(head, sizeof head, "%s %s", c->name, c->args);
        kprintf("  %-22s %s\n", head, c->help);
    }
}
static void c_clear(char *a) { (void)a; vga_clear(); }
static void c_echo(char *a) { kprintf("%s\n", skip(a)); }
static void c_about(char *a) {
    (void)a;
    rtc_time_t t; rtc_read(&t);
    color(VGA_LCYAN);
    kprintf("   _   _____   __ \n  | | / / _ | / /    %s v%s \"%s\"\n", VUKANG_NAME, VUKANG_VERSION, VUKANG_CODENAME);
    kprintf("  | |/ / __ |/ /__   Arch    : x86 (i386), 32-bit protected mode\n");
    kprintf("  |___/_/ |_/____/   Kernel  : monolithic, written in C + NASM assembly\n");
    color(VGA_LGREY);
    kprintf("                     Memory  : %u KiB RAM, paging on, 8 MiB heap\n", pmm_mem_kb());
    kprintf("                     Storage : %s\n", blockdev_get()->name);
    if (vga_is_graphics()) kprintf("                     Display : %ux%u framebuffer, %d bpp, %dx%d console\n", fb.width, fb.height, fb.bpp, vga_cols(), vga_rows());
    else                   kprintf("                     Display : VGA text mode 80x25\n");
    kprintf("                     FS / DB : VKFS + VukangDB (SQL)\n");
    kprintf("                     Uptime  : %u s   Date: %04d-%02d-%02d\n", timer_ticks() / timer_hz(), t.year, t.month, t.day);
    kprintf("                     Author  : VukangTech - Dar es Salaam, Tanzania\n");
}
static void c_mem(char *a) {
    (void)a;
    uint32_t tot, used, blocks, hs, he; heap_stats(&tot, &used, &blocks); heap_range(&hs, &he);
    uint32_t pt = pmm_total_pages(), pf = pmm_free_pages();
    kprintf("Physical : %u KiB total, %u KiB used, %u KiB free (%u frames)\n", pt * 4, (pt - pf) * 4, pf * 4, pt);
    kprintf("Heap     : %u KiB at %p-%p, %u KiB used, %u blocks, integrity %s\n",
            tot / 1024, hs, he, used / 1024, blocks, heap_check() ? "BROKEN" : "ok");
    kprintf("Paging   : %u MiB kernel map (supervisor-only), user space %p-%p\n", paging_mapped_mb(), USER_BASE, USER_END);
    kprintf("Rejected : %u bad frame frees, %u bad heap frees\n", pmm_rejected_frees(), heap_rejected_frees());
}
static void c_uptime(char *a) {
    (void)a; uint32_t s = timer_ticks() / timer_hz();
    kprintf("up %u h %u min %u s  (%u ticks)\n", s / 3600, (s / 60) % 60, s % 60, timer_ticks());
}
static void c_date(char *a) {
    (void)a; rtc_time_t t; rtc_read(&t);
    kprintf("%04d-%02d-%02d %02d:%02d:%02d UTC\n", t.year, t.month, t.day, t.hour, t.minute, t.second);
}
static void c_reboot(char *a) { (void)a; kprintf("Rebooting...\n"); power_reboot(); }
static void c_halt(char *a) { (void)a; kprintf("Shutting down. Kwaheri!\n"); power_shutdown(); }
static void c_beep(char *a) { (void)a; pcspk_beep(880, 150); pcspk_beep(660, 150); }
static void c_color(char *a) {
    int c = atoi(skip(a));
    if (c < 1 || c > 15) { kprintf("usage: color <1-15>\n"); return; }
    vga_set_color(c, VGA_BLACK); kprintf("Color changed.\n");
}

/* ---------------- processes ---------------- */
static const char *st_name(task_state_t s) {
    switch (s) { case T_READY: return "ready"; case T_RUNNING: return "running";
                 case T_SLEEPING: return "sleeping"; case T_DEAD: return "dead"; default: return "?"; }
}
static void c_ps(char *a) {
    (void)a; task_t *t = task_table();
    kprintf("  PID  MODE    STATE      CPU-TICKS  USER-PAGES  NAME\n");
    for (int i = 0; i < MAX_TASKS; i++)
        if (t[i].state != T_UNUSED && t[i].state != T_DEAD)
            kprintf("  %3d  %-6s  %-9s  %9u  %10u  %s\n", t[i].pid, t[i].user ? "ring3" : "kernel",
                    st_name(t[i].state), t[i].cpu_ticks, t[i].user ? vmm_user_pages(t[i].pd) : 0, t[i].name);
}
static void worker(void *arg) {
    uint32_t limit = (uint32_t)arg, found = 0, start = timer_ticks();
    for (uint32_t n = 2; n < limit; n++) {
        int prime = 1;
        for (uint32_t d = 2; d * d <= n; d++) if (n % d == 0) { prime = 0; break; }
        found += prime;
    }
    kprintf("\n[pid %d] finished: %u primes below %u in %u ms\n", task_getpid(), found, limit, (timer_ticks() - start) * 10);
}
static void c_spawn(char *a) {
    int n = atoi(skip(a)); if (n <= 0) n = 3; if (n > 8) n = 8;
    for (int i = 0; i < n; i++) {
        int pid = task_create("prime-worker", worker, (void *)(200000 + i * 100000));
        kprintf("spawned worker pid %d\n", pid);
    }
    kprintf("They run in the background. Use 'ps' to watch them.\n");
}
static void c_kill(char *a) {
    int pid = atoi(skip(a)), r = task_kill(pid);
    kprintf(r == 0 ? "killed %d\n" : r == -2 ? "cannot kill the kernel (pid %d)\n" : "no such pid %d\n", pid);
}
static void clockd(void *arg) {           /* background daemon: live clock in top-right corner */
    (void)arg;
    for (;;) {
        rtc_time_t t; rtc_read(&t);
        char buf[12]; ksnprintf(buf, sizeof buf, " %02d:%02d:%02d ", t.hour, t.minute, t.second);
        uint32_t f = irq_save();
        vga_write_at(vga_cols() - 10, 0, buf, VGA_BLACK | (VGA_LCYAN << 4));
        irq_restore(f);
        task_sleep(500);
    }
}
static void c_progs(char *a) {
    (void)a; kprintf("User programs (ELF, run in ring 3 with a private address space):\n");
    for (const user_prog_t *p = user_prog_list(); p->name; p++) kprintf("  %-16s %u bytes\n", p->name, p->size);
}
static void c_run(char *a) {
    char *name = word(&a); int bg = !strcmp(skip(a), "&");
    if (!name[0]) { kprintf("usage: run <program> [&]   (see 'progs')\n"); return; }
    int pid = process_spawn(name);
    if (pid < 0) { kprintf("run: %s: %s\n", name, pid == -1 ? "no such program" : pid == -3 ? "invalid ELF" : "out of memory"); return; }
    if (bg) { task_detach(pid); kprintf("[%d] %s started in background\n", pid, name); return; }
    int code, r;
    while ((r = task_try_wait(pid, &code)) == 0) {      /* Ctrl+C kills the foreground process */
        int c = keyboard_getc_nonblock();
        if (c < 0 && serial_received()) c = serial_getc();
        if (c == 3) { task_kill(pid); kprintf("^C\n"); }
        task_sleep(20);
    }
    kprintf("[%d] %s exited with code %d%s\n", pid, name, code, code == EXIT_KILLED ? " (killed)" : "");
}
static void c_security(char *a) {
    (void)a;
    static const struct { const char *prog; int expect; const char *what; } tests[] = {
        { "attack_read",    EXIT_PAGEFAULT, "read kernel memory" },
        { "attack_write",   EXIT_PAGEFAULT, "overwrite kernel memory" },
        { "attack_exec",    EXIT_PAGEFAULT, "execute kernel code" },
        { "attack_vga",     EXIT_PAGEFAULT, "touch hardware memory" },
        { "attack_cli",     EXIT_GPF,       "privileged instruction" },
        { "attack_syscall", 0,              "leak memory via syscall" },
    };
    int ok = 0, n = sizeof tests / sizeof tests[0];
    color(VGA_YELLOW); kprintf("VUKANG OS security suite: %d attacks from ring 3\n", n); color(VGA_LGREY);
    for (int i = 0; i < n; i++) {
        kprintf("\n(%d/%d) %s\n", i + 1, n, tests[i].what);
        int pid = process_spawn(tests[i].prog);
        int code = pid < 0 ? 999 : task_wait(pid);
        int held = code == tests[i].expect;
        ok += held;
        color(held ? VGA_LGREEN : VGA_LRED);
        kprintf("  => %s (exit %d)\n", held ? "BLOCKED - kernel protected" : "NOT BLOCKED", code);
        color(VGA_LGREY);
    }
    color(ok == n ? VGA_LGREEN : VGA_LRED);
    kprintf("\n%d/%d protections held. Kernel still running.\n", ok, n);
    color(VGA_LGREY);
}
static void c_selftest(char *a) { (void)a; extern int kernel_selftest(void); kernel_selftest(); }

/* ---------------- files ---------------- */
static void c_ls(char *a) {
    (void)a;
    vkfs_entry_t *e = kmalloc(sizeof(vkfs_entry_t) * VKFS_MAX_FILES);
    if (!e) { kprintf("ls: out of memory\n"); return; }
    int n = vkfs_list(e, VKFS_MAX_FILES);
    kprintf("  %-28s %8s  %s\n", "NAME", "BYTES", "MODIFIED");
    for (int i = 0; i < n; i++) {
        uint32_t m = e[i].mtime;
        kprintf("  %-28s %8u  %04u-%02u-%02u %02u:%02u\n", e[i].name, e[i].size,
                2000 + (m >> 26), (m >> 22) & 15, (m >> 17) & 31, (m >> 12) & 31, (m >> 6) & 63);
    }
    kprintf("  %d file(s)\n", n);
    kfree(e);
}
static void c_cat(char *a) {
    char *name = word(&a);
    int sz = vkfs_size(name);
    if (sz < 0) { kprintf("cat: %s: no such file\n", name); return; }
    char *buf = kmalloc(sz + 1);
    if (!buf) { kprintf("cat: out of memory\n"); return; }
    if (vkfs_read(name, buf, sz) != sz) { kprintf("cat: read error\n"); kfree(buf); return; }
    buf[sz] = 0;
    for (int i = 0; i < sz; i++) console_putc((buf[i] >= 32 && buf[i] < 127) || buf[i] == '\n' ? buf[i] : '.');
    if (sz && buf[sz - 1] != '\n') console_putc('\n');
    kfree(buf);
}
static void c_write(char *a) {
    char *name = word(&a); char *text = skip(a);
    if (!name[0]) { kprintf("usage: write <file> <text>\n"); return; }
    char *buf = kmalloc(strlen(text) + 2);
    if (!buf) { kprintf("write: out of memory\n"); return; }
    strcpy(buf, text); strcat(buf, "\n");
    kprintf(vkfs_write(name, buf, strlen(buf)) ? "wrote %s\n" : "write failed: %s\n", name);
    kfree(buf);
}
static void c_append(char *a) {
    char *name = word(&a); char *text = skip(a);
    int sz = vkfs_size(name); if (sz < 0) sz = 0;
    if (!name[0]) { kprintf("usage: append <file> <text>\n"); return; }
    char *buf = kmalloc(sz + strlen(text) + 2);
    if (!buf) { kprintf("append: out of memory\n"); return; }
    if (sz && vkfs_read(name, buf, sz) != sz) { kprintf("append: read error\n"); kfree(buf); return; }
    memcpy(buf + sz, text, strlen(text)); buf[sz + strlen(text)] = '\n';
    vkfs_write(name, buf, sz + strlen(text) + 1);
    kfree(buf);
}
static void c_rm(char *a) { char *n = word(&a); kprintf(vkfs_delete(n) ? "removed %s\n" : "rm: %s: no such file\n", n); }
static void c_df(char *a) {
    (void)a; uint32_t t, f, n; vkfs_info(&t, &f, &n);
    kprintf("VKFS on %s\n  size %u KiB, free %u KiB, used %u%%, files %u/%d\n",
            blockdev_get()->name, t / 2, f / 2, t ? 100 - (f * 100) / t : 0, n, VKFS_MAX_FILES);
}
static void c_format(char *a) {
    if (strcmp(skip(a), "yes")) { kprintf("This erases the disk. Type: format yes\n"); return; }
    kprintf(vkfs_format() ? "Disk formatted with VKFS.\n" : "Format failed.\n");
}

/* ---------------- database ---------------- */
static void c_sql(char *a) { db_exec(skip(a)); }
static void c_db(char *a) { (void)a; db_repl(); }

/* ---------------- presentation demo ---------------- */
static void step(const char *title) { color(VGA_YELLOW); kprintf("\n>>> %s\n", title); color(VGA_LGREY); timer_sleep(400); }
static void run_line(const char *line) {
    char buf[LINE]; strncpy(buf, line, LINE - 1); buf[LINE - 1] = 0;
    color(VGA_LGREEN); kprintf("vukang$ "); color(VGA_WHITE); kprintf("%s\n", line); color(VGA_LGREY);
    char *p = buf; char *w = word(&p);
    const cmd_t *c = find_cmd(w);
    if (c) c->fn(p);
    timer_sleep(300);
}
static void c_demo(char *a) {
    (void)a;
    step("1. System information");           run_line("about");
    step("2. Memory management");            run_line("mem");
    step("3. Filesystem (VKFS)");            run_line("write hello.txt Habari kutoka VUKANG OS!"); run_line("cat hello.txt"); run_line("ls");
    step("4. Database engine (VukangDB)");
    db_exec("DROP TABLE wanafunzi");
    run_line("sql CREATE TABLE wanafunzi (id INT PRIMARY KEY, jina TEXT, mji TEXT, alama INT)");
    run_line("sql INSERT INTO wanafunzi VALUES (1,'Asha','Dar es Salaam',88),(2,'Juma','Arusha',75),(3,'Neema','Mwanza',93),(4,'Baraka','Dodoma',67)");
    run_line("sql SELECT * FROM wanafunzi ORDER BY alama DESC");
    run_line("sql SELECT AVG(alama) FROM wanafunzi");
    run_line("sql SELECT jina, alama FROM wanafunzi WHERE alama >= 80");
    step("5. Transactions (BEGIN / ROLLBACK)");
    run_line("sql BEGIN; DELETE FROM wanafunzi; SELECT COUNT(*) FROM wanafunzi");
    run_line("sql ROLLBACK; SELECT COUNT(*) FROM wanafunzi");
    step("6. Multitasking");                 run_line("spawn 2"); run_line("ps");
    step("7. User process in its own address space"); run_line("run hello");
    step("8. Attacks from ring 3 vs. kernel protection"); run_line("security");
    timer_sleep(1500);
    color(VGA_LCYAN); kprintf("\nDemo complete. Asanteni!\n"); color(VGA_LGREY);
}

const cmd_t commands[] = {
    { "help", "", "show this list", c_help },
    { "about", "", "system information", c_about },
    { "clear", "", "clear the screen", c_clear },
    { "echo", "<text>", "print text", c_echo },
    { "mem", "", "memory usage (frames, heap, paging)", c_mem },
    { "uptime", "", "time since boot", c_uptime },
    { "date", "", "real-time clock", c_date },
    { "ps", "", "list running tasks", c_ps },
    { "spawn", "[n]", "start n background worker tasks", c_spawn },
    { "kill", "<pid>", "stop a task", c_kill },
    { "progs", "", "list user programs (ELF)", c_progs },
    { "run", "<prog> [&]", "start a ring-3 process", c_run },
    { "security", "", "run 6 attacks against the kernel", c_security },
    { "usermode", "", "alias for 'security'", c_security },
    { "selftest", "", "kernel memory self-tests", c_selftest },
    { "ls", "", "list files on VKFS", c_ls },
    { "cat", "<file>", "show a file", c_cat },
    { "write", "<file> <text>", "create/overwrite a file", c_write },
    { "append", "<file> <text>", "append a line to a file", c_append },
    { "rm", "<file>", "delete a file", c_rm },
    { "df", "", "disk usage", c_df },
    { "format", "yes", "erase disk and create VKFS", c_format },
    { "sql", "<statement>", "run one SQL statement", c_sql },
    { "db", "", "open the VukangDB SQL console", c_db },
    { "demo", "", "guided showcase (for presentations)", c_demo },
    { "color", "<1-15>", "change text color", c_color },
    { "beep", "", "PC speaker sound", c_beep },
    { "reboot", "", "restart the computer", c_reboot },
    { "halt", "", "power off", c_halt },
    { 0, 0, 0, 0 }
};
static const cmd_t *find_cmd(const char *n) {
    for (const cmd_t *c = commands; c->name; c++) if (!strcmp(c->name, n)) return c;
    return 0;
}

void shell_run(void) {
    task_create("clockd", clockd, 0);
    char line[LINE];
    for (;;) {
        color(VGA_LGREEN); kprintf("vukang");
        color(VGA_WHITE);  kprintf(":");
        color(VGA_LBLUE);  kprintf("~");
        color(VGA_WHITE);  kprintf("$ ");
        color(VGA_LGREY);
        console_readline(line, sizeof line);
        char *p = line; char *w = word(&p);
        if (!w[0]) continue;
        const cmd_t *c = find_cmd(w);
        if (c) c->fn(p);
        else kprintf("%s: command not found (try 'help')\n", w);
    }
}
