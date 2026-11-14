/* attack_syscall: trick the kernel into leaking kernel memory via write() */
#include "ulib.h"
int main(void) {
    int bad = 0;
    print("  [attack] asking kernel to print memory at 0x00100000 (kernel)...\n");
    int r = sys_write((const char *)0x00100000);
    print("  [attack] kernel answered "); print_int(r); print(r == -14 ? " (EFAULT: refused)\n" : " (LEAKED!)\n");
    bad |= r != -14;
    print("  [attack] asking kernel to print unmapped address 0x50000000...\n");
    r = sys_write((const char *)0x50000000);
    print("  [attack] kernel answered "); print_int(r); print(r == -14 ? " (EFAULT: refused)\n" : " (BUG!)\n");
    bad |= r != -14;
    return bad;
}
