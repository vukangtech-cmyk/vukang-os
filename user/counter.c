/* counter: long-running process to show several address spaces at once */
#include "ulib.h"
static int value;                        /* same virtual address in every copy, different RAM */
int main(void) {
    int pid = sys_getpid();
    for (value = 1; value <= 5; value++) {
        print("  [counter pid "); print_int(pid); print("] "); print_int(value);
        print("  (&value = "); print_hex((uint32_t)&value); print(")\n");
        sys_sleep(700);
    }
    return 0;
}
