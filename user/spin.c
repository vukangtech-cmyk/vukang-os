/* spin: never yields voluntarily - the timer still preempts it.
 * Try: run spin &   then: ps   then: kill <pid>   (or Ctrl+C in foreground) */
#include "ulib.h"
int main(void) {
    print("  [spin] burning CPU forever; the scheduler will still share the CPU\n");
    volatile uint32_t x = 0;
    for (;;) x++;
    return 0;
}
