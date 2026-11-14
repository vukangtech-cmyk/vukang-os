/* attack_read: try to READ kernel memory (the kernel image at 1 MiB) */
#include "ulib.h"
int main(void) {
    volatile uint32_t *kernel = (uint32_t *)0x00100000;
    print("  [attack] reading kernel memory at 0x00100000...\n");
    uint32_t v = *kernel;
    print("  [attack] SUCCESS?! read "); print_hex(v); print(" - protection is BROKEN\n");
    return 1;
}
