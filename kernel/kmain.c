/* =============================================================
 *  VUKANG OS - kernel/kmain.c
 *  Kernel entry: brings up every subsystem in order, then
 *  starts the interactive shell.
 * ============================================================= */
#include <stdarg.h>
#include "kernel.h"
#include "console.h"
#include "vga.h"
#include "serial.h"
#include "arch.h"
#include "mm.h"
#include "task.h"
#include "drivers.h"
#include "fs.h"
#include "db.h"
#include "io.h"
#include "fb.h"
#include "splash.h"
#include "multiboot.h"
#include "string.h"

void klog(const char *tag, const char *fmt, ...) {
    char buf[256]; va_list ap;
    va_start(ap, fmt); kvsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    uint8_t c = vga_get_color();
    console_write("[");
    vga_set_color(VGA_LGREEN, VGA_BLACK); console_write(" OK ");
    vga_set_color(c & 0x0F, c >> 4);
    kprintf("] %-8s %s\n", tag, buf);
}

/* ---- kernel command line (from GRUB), e.g. "nosplash" ---- */
static char cmdline[256];
bool kernel_arg(const char *name) {
    size_t n = strlen(name);
    for (const char *p = cmdline; *p; ) {
        while (*p == ' ') p++;
        const char *e = p; while (*e && *e != ' ') e++;
        if ((size_t)(e - p) == n && !strncmp(p, name, n)) return true;
        p = e;
    }
    return false;
}
const char *kernel_cmdline(void) { return cmdline; }

void panic(const char *msg, const char *file, int line) {
    __asm__ volatile("cli");
    splash_abort();                   /* make sure the message is visible */
    vga_set_color(VGA_WHITE, VGA_RED);
    kprintf("\n KERNEL PANIC: %s\n   at %s:%d\n   System halted.\n", msg, file, line);
    for (;;) __asm__ volatile("hlt");
}

static void banner(void) {
    vga_set_color(VGA_LCYAN, VGA_BLACK);
    kprintf("\n");
    kprintf("  __      ___   _ _  __    _    _   _  ____     ___  ____  \n");
    kprintf("  \\ \\    / / | | | |/ /   / \\  | \\ | |/ ___|   / _ \\/ ___| \n");
    kprintf("   \\ \\  / /| | | | ' /   / _ \\ |  \\| | |  _   | | | \\___ \\ \n");
    kprintf("    \\ \\/ / | |_| | . \\  / ___ \\| |\\  | |_| |  | |_| |___) |\n");
    kprintf("     \\__/   \\___/|_|\\_\\/_/   \\_\\_| \\_|\\____|   \\___/|____/ \n");
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    kprintf("\n   %s v%s \"%s\"  -  Made in Tanzania\n\n", VUKANG_NAME, VUKANG_VERSION, VUKANG_CODENAME);
    vga_set_color(VGA_LGREY, VGA_BLACK);
}

void kmain(uint32_t magic, void *mbinfo) {
    if (magic == MULTIBOOT_MAGIC) {   /* read boot info while paging is still off */
        multiboot_info_t *mb = mbinfo;
        fb_probe(mb);
        if ((mb->flags & MB_INFO_CMDLINE) && mb->cmdline) {
            const char *c = (const char *)mb->cmdline;
            size_t i = 0; while (c[i] && i < sizeof cmdline - 1) { cmdline[i] = c[i]; i++; }
            cmdline[i] = 0;
        }
    }
    bool anim = !kernel_arg("nosplash");
    if (anim) vga_hold(true);         /* draw nothing until the boot animation takes over */
    vga_init();
    serial_init();
    banner();

    gdt_init();                       klog("cpu", "GDT + TSS loaded (ring 0 / ring 3 segments)");
    idt_init();                       klog("cpu", "IDT ready: 32 exceptions, 16 IRQs, syscall int 0x80");
    pmm_init(magic, mbinfo);          klog("memory", "%u KiB RAM, %u frames, kernel+BIOS+boot data locked", pmm_mem_kb(), pmm_total_pages());
    paging_init();                    klog("memory", "paging on: %u MiB kernel map (supervisor-only), WP, NULL guard", paging_mapped_mb());
    heap_init();
    { uint32_t hs, he; heap_range(&hs, &he);
                                      klog("memory", "heap %u MiB at %p-%p (locked, inside map)", (he - hs) >> 20, hs, he); }
    tasking_init();
    syscall_init();                   { int np = 0; for (const user_prog_t *u = user_prog_list(); u->name; u++) np++;
                                      klog("proc", "scheduler + idle task, ELF loader, %d user programs", np); }
    timer_init(100);                  klog("timer", "PIT at 100 Hz, preemptive scheduler armed");
    keyboard_init();                  klog("input", "PS/2 keyboard + serial console");
    __asm__ volatile("sti");
    if (vga_is_graphics()) klog("display", "%ux%u, %u-bit framebuffer, %dx%d text console", fb.width, fb.height, fb.bpp, vga_cols(), vga_rows());
    else                   klog("display", "VGA text mode 80x25");

    if (anim) splash_begin();         /* intro animation, console logs silently meanwhile */
    splash_step(40, "Kernel and memory protection ready");

    blockdev_init();
    blockdev_t *bd = blockdev_get();
    klog("storage", "%s, %u sectors (%u MiB)", bd->name, bd->sectors, bd->sectors / 2048);
    if (ata_present() && ata_model()[0]) klog("storage", "model: %s", ata_model());
    splash_step(60, "Storage ready");
    if (vkfs_mount()) {
        uint32_t t, f, n; vkfs_info(&t, &f, &n);
        klog("vkfs", "mounted: %u files, %u KiB free", n, f / 2);
    } else {
        vkfs_format();
        klog("vkfs", "no filesystem found - formatted new VKFS volume");
    }
    splash_step(80, "VKFS file system ready");
    db_init();                        klog("db", "VukangDB SQL engine online");
    splash_step(95, "VukangDB online");
    splash_end();

    kprintf("\nType 'help' to see commands. Karibu!\n\n");
    shell_run();
}
