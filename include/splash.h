#ifndef VK_SPLASH_H
#define VK_SPLASH_H
/* VUKANG OS - animated boot splash.
 * splash_begin() plays the intro, splash_step() moves the progress bar to a
 * real boot stage, splash_end() finishes, fades out and shows the console.
 * Any key skips the animation. Kernel argument "nosplash" disables it. */
#include <stdbool.h>
void splash_begin(void);
void splash_step(int percent, const char *label);
void splash_end(void);
bool splash_active(void);
void splash_abort(void);                 /* panic path: drop the splash immediately */
#endif
