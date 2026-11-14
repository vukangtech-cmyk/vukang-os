/* attack_write: try to OVERWRITE kernel memory */
#include "ulib.h"
int main(void) {
    print("  [attack] writing 0xDEADBEEF over the kernel at 0x00101000...\n");
    *(volatile uint32_t *)0x00101000 = 0xDEADBEEF;
    print("  [attack] SUCCESS?! kernel overwritten - protection is BROKEN\n");
    return 1;
}
