/* hello: first user program - shows syscalls inside a private address space */
#include "ulib.h"
static int counter = 7;                  /* .data  : private, writable */
static char buffer[64];                  /* .bss   : zero-filled by the loader */
int main(void) {
    print("  [ring3] Habari! I am a user process in my own address space.\n");
    print("  [ring3] pid = "); print_int(sys_getpid());
    print(", code at "); print_hex((uint32_t)main);
    print(", stack at "); print_hex((uint32_t)__builtin_frame_address(0)); print("\n");
    counter *= 6; buffer[0] = 'O'; buffer[1] = 'K'; buffer[2] = '\n';
    print("  [ring3] my private data: counter = "); print_int(counter); print(", buffer = "); print(buffer);
    int t0 = sys_ticks(); sys_sleep(200);
    print("  [ring3] slept "); print_int((sys_ticks() - t0) * 10); print(" ms. Kwaheri!\n");
    return 42;
}
