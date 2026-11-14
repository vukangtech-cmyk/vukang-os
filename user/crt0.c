/* VUKANG OS user program entry point */
#include "ulib.h"
__attribute__((section(".text.start"))) void _start(void) { sys_exit(main()); }
