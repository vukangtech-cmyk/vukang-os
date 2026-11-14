# Maintenance ya VUKANG OS

Hivi ndivyo tutakavyoendeleza mradi kwa nidhamu.

## Mtiririko wa kazi (Git workflow)

1. `main` ni tawi linalofanya kazi siku zote. Majaribio lazima yapite.
2. Kila kipengele kipya kinafanyika kwenye tawi lake: `feature/b-tree-index`, `fix/ata-timeout`.
3. Unafungua Pull Request. GitHub Actions inajenga ISO na kuendesha checks 30 kiotomatiki (pamoja na `selftest` na `security`).
4. Una-merge PR baada ya CI kuwa kijani.

```bash
git checkout -b feature/jina-la-kipengele
# ... badilisha msimbo ...
make test
git commit -am "db: ongeza JOIN"
git push -u origin feature/jina-la-kipengele
gh pr create
```

## Matoleo (Semantic Versioning)

`MAJOR.MINOR.PATCH`, kwa mfano `0.2.0`:
- PATCH (0.1.1): kurekebisha makosa
- MINOR (0.2.0): vipengele vipya
- MAJOR (1.0.0): mabadiliko makubwa, mfano muundo mpya wa VKFS

Kwa kila toleo: badilisha `VUKANG_VERSION` ndani ya `include/kernel.h`, andika `CHANGELOG.md`, kisha `git tag v0.2.0 && git push --tags`. Workflow `release.yml` itajenga ISO, kuipima na kuiweka kwenye GitHub Releases yenyewe. Kila release ina jina la mlima au mbuga ya Tanzania: Kilimanjaro, Serengeti, Ngorongoro, Meru...

## Kuongeza amri mpya ya shell

Ndani ya `shell/shell.c`:
```c
static void c_hello(char *a) { kprintf("Habari %s\n", a); }
// ndani ya commands[]:
{ "hello", "<jina>", "salamu", c_hello },
```

## Kuongeza programu mpya ya user (ring 3)

1. Andika `user/jina.c` ukitumia `user/ulib.h` (mfano `sys_write`, `sys_sleep`, `sys_exit`). Programu inaanzia `main()`; `crt0.c` inaiita na kufanya `exit`.
2. Ongeza `jina` kwenye `USER_PROGS` ndani ya `Makefile`.
3. `make run` kisha `progs` na `run jina`. Programu inaunganishwa kwenye `0x40000000` na inapata address space yake.

## Kuongeza syscall mpya

1. Ongeza namba kwenye `enum` ndani ya `kernel/proc/syscall.c`.
2. Ongeza `case` ndani ya `syscall_dispatch`.
3. Kila pointer kutoka user lazima ikaguliwe kwa `vmm_user_range_ok()` / `vmm_user_strlen()` kabla ya kuisoma; rudisha `-EFAULT` kama si halali.
4. Andika programu ya ring 3 inayoitumia, na jaribio kwenye `scripts/qemu_test.py`.

## Boot animation: kuipima na kuibadilisha

Animation hii itatumika kwenye matoleo yote mpaka tuamue kubadilisha picha.

**Kuipima:**
- `make run`: chagua `VUKANG OS` kwenye menu ya GRUB. Animation inachukua takriban sekunde 3.5, kisha console inaonekana.
- Bonyeza kitufe chochote wakati wa animation: inarukwa mara moja.
- Chaguo `bila animation (nosplash)` na `text mode` lazima zifike kwenye shell bila tatizo.
- Ndani ya log ya kuwaka (au `make run-serial`) tafuta mstari `splash boot animation: N frames in M ms (X fps)`.
- `make test` ina checks 4 za display: framebuffer console, animation imecheza, text mode, na nosplash.

**Kubadilisha picha (kwa toleo la baadaye):**
1. Weka picha mpya kwenye `assets/splash/logo.jpg` (mandharinyuma meusi ni bora).
2. Rekebisha crop boxes za `emblem` na `wordmark` ndani ya `tools/mksplash.py` kama nafasi za nembo zimebadilika.
3. `make splash` (inahitaji Python Pillow: `pip install pillow`). Itatengeneza upya `emblem.vkimg` na `wordmark.vkimg`.
4. `make run` kuangalia, kisha commit picha na `.vkimg` pamoja.

Muda, rangi na idadi ya chembe ziko juu ya `kernel/splash.c`. Font inabadilishwa kwa `make font` (`tools/mkfont.py`, inasoma font ya PSF).

## Kanuni za usalama wa memory

- Usiweke `PG_USER` nje ya `USER_BASE..USER_END` (`vmm_map` itakataa).
- Kagua status ya `pmm_free()`; frames za kudumu zifungwe kwa `pmm_lock()`.
- Kagua kila `kmalloc()` dhidi ya `NULL`.
- Kabla ya kila release: `selftest` iwe 23/23 na `security` iwe 6/6.

## Kanuni za msimbo

- C11, bila libc. Tumia `lib/string.c` na `kprintf`.
- Kila faili linaanza na maoni yanayoeleza kazi yake.
- Usiwe na warning yoyote: tunajenga kwa `-Wall -Wextra`.
- Hakuna division ya namba za 64-bit (hakuna libgcc).
- Code inayogusa data inayoshirikiwa na interrupts inalindwa kwa `irq_save()` / `irq_restore()`.

## Kutafuta makosa (debugging)

| Dalili | Chanzo kinachowezekana | Zana |
|---|---|---|
| QEMU inajiwasha upya mfululizo | triple fault: IDT/GDT mbaya au stack imeharibika | `qemu-system-i386 -d int,cpu_reset -no-reboot` |
| "KERNEL EXCEPTION Page Fault" | pointer mbaya. CR2 inaonyesha anwani | angalia EIP kwa `addr2line -e build/vukang.kernel <EIP>` |
| "heap buffer overflow detected (canary smashed)" | kuandika nje ya buffer ya `kmalloc` | angalia block iliyoachiliwa mwisho |
| "[heap] rejected kfree" | pointer ya kigeni au double free | tafuta `kfree` inayoitwa mara mbili |
| "[pid N ...] Page Fault ... -> process killed" | programu ya user imegusa anwani isiyoruhusiwa | ujumbe unaonyesha anwani na aina ya kosa |
| Disk haionekani | QEMU haina `-drive` | tumia `make run` |

GDB: `qemu-system-i386 -s -S -cdrom build/vukang-os.iso`, kisha `gdb build/vukang.kernel -ex "target remote :1234"`.

## Ratiba ya maintenance

- Kila wiki: kipengele kimoja kutoka ROADMAP, kwenye tawi lake na PR.
- Kila mwezi: toleo jipya (MINOR) pamoja na CHANGELOG.
- Kabla ya presentation: tawi la `release/x.y`, ISO kwenye GitHub Releases, na video ya demo.
