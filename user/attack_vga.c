/* attack_vga: try to write the screen directly, bypassing the kernel */
#include "ulib.h"
int main(void) {
    print("  [attack] writing directly to VGA memory 0x000B8000...\n");
    *(volatile uint16_t *)0xB8000 = 0x4F21;
    print("  [attack] SUCCESS?! wrote to hardware memory - protection is BROKEN\n");
    return 1;
}
