/* VUKANG OS - kernel/proc/programs.c : registry of user programs
 * The ELF files are built from the user/ C sources and embedded into the kernel by the
 * Makefile (build/user/programs.asm). A future version loads them from VKFS. */
#include "task.h"
#include "string.h"
extern const user_prog_t user_programs[];
const user_prog_t *user_prog_list(void) { return user_programs; }
const user_prog_t *user_prog_find(const char *name) {
    for (const user_prog_t *p = user_programs; p->name; p++) if (!strcmp(p->name, name)) return p;
    return 0;
}
