/* attack_cli: try a privileged instruction (disable interrupts) */
#include "ulib.h"
int main(void) {
    print("  [attack] executing privileged instruction 'cli'...\n");
    __asm__ volatile("cli");
    print("  [attack] SUCCESS?! interrupts disabled - protection is BROKEN\n");
    return 1;
}
