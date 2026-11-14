# Maintenance ya VUKANG OS

Hivi ndivyo tutakavyoendeleza mradi kwa nidhamu.

## Mtiririko wa kazi (Git workflow)

1. `main` ni tawi linalofanya kazi siku zote. Majaribio lazima yapite.
2. Kila kipengele kipya kinafanyika kwenye tawi lake: `feature/b-tree-index`, `fix/ata-timeout`.
3. Unafungua Pull Request. GitHub Actions inajenga ISO na kuendesha majaribio 16 kiotomatiki.
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

Kwa kila toleo: badilisha `VUKANG_VERSION` ndani ya `include/kernel.h`, andika `CHANGELOG.md`, kisha `git tag v0.2.0 && git push --tags`. Kila release ina jina la mlima au mbuga ya Tanzania: Kilimanjaro, Serengeti, Ngorongoro, Meru...

## Kuongeza amri mpya ya shell

Ndani ya `shell/shell.c`:
```c
static void c_hello(char *a) { kprintf("Habari %s\n", a); }
// ndani ya commands[]:
{ "hello", "<jina>", "salamu", c_hello },
```

## Kuongeza syscall mpya

1. Ongeza namba kwenye `enum` ndani ya `kernel/proc/syscall.c`.
2. Ongeza `case` ndani ya `syscall_dispatch`.
3. Andika jaribio la ring 3 linaloitumia.

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
| "heap corruption detected" | kuandika nje ya buffer | angalia `kmalloc` iliyo karibu |
| Disk haionekani | QEMU haina `-drive` | tumia `make run` |

GDB: `qemu-system-i386 -s -S -cdrom build/vukang-os.iso`, kisha `gdb build/vukang.kernel -ex "target remote :1234"`.

## Ratiba ya maintenance

- Kila wiki: kipengele kimoja kutoka ROADMAP, kwenye tawi lake na PR.
- Kila mwezi: toleo jipya (MINOR) pamoja na CHANGELOG.
- Kabla ya presentation: tawi la `release/x.y`, ISO kwenye GitHub Releases, na video ya demo.
