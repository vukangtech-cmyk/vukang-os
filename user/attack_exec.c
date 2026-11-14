/* attack_exec: try to JUMP into kernel code */
#include "ulib.h"
int main(void) {
    print("  [attack] jumping into kernel code at 0x00100000...\n");
    ((void (*)(void))0x00100000)();
    print("  [attack] SUCCESS?! executed kernel code - protection is BROKEN\n");
    return 1;
}
