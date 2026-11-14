# Changelog

## [0.2.2] "Serengeti" - 2026-11-14
Toleo la usalama wa data. Maelezo kamili: [docs/SECURITY.md](docs/SECURITY.md) (sehemu ya v0.2.2).

### Imerekebishwa
- VKFS: disk ikijaa, table (faili) ilifutwa. `vkfs_write` sasa inahifadhi extent ya zamani mpaka data mpya iwe salama (`extend_extent`, `alloc_extent` bila kuachilia), na `sb.files` inaongezwa kwa faili jipya tu
- VukangDB: INSERT ya rows nyingi iliyoshindwa iliacha rows nusu (hata ndani ya BEGIN/ROLLBACK). Sasa ni yote au hakuna (`insert_tuples`, `undo_rows`, `MAX_STMT_ROWS` 64)
- VukangDB: `tbl_flush` ilipanga RAM upya kabla ya kuandika. Sasa inapanga baada ya mafanikio tu; `tbl_discard` inatupa nakala ya RAM kuandika kukishindwa
- VukangDB: UPDATE iliweza kuvunja PRIMARY KEY. Sasa ina hatua mbili (kukagua, kisha kutumia)

### Imeongezwa
- `scripts/integrity_test.py` (checks 12) na `make test-integrity`; `make test` sasa ni checks 42
- `make versions`, `make run-version V=<tag>` na wenzake (`scripts/version.sh`): kuwasha toleo lolote, kila moja kwenye worktree na disk yake

## [0.2.1] "Serengeti" - 2026-11-14
Toleo la sura mpya: boot animation na console ya graphics.

### Imeongezwa
- Boot animation ya kisasa yenye motion, inayotumia nembo rasmi ya VUKANG: nembo inakuza na kung'aa, jina linapanda, chembe za mwanga, progress bar inayofuata hatua halisi za kuwaka, na fade-out. Takriban fps 98 kwenye QEMU
- Kitufe chochote kinaruka animation; chaguo la GRUB `nosplash`; text mode kwa kompyuta zisizo na framebuffer
- Framebuffer driver (`drivers/fb.c`), 1024x768x32, imewekwa supervisor-only
- Console mpya ya graphics: font ya 10x20 (Terminus Bold, SIL OFL 1.1), 102x38, palette ya kisasa, damage tracking
- Menu ya GRUB yenye graphics na chaguo 3
- `tools/mksplash.py`, `tools/mkfont.py`, `make splash`, `make font`
- `make test` checks 30 (4 mpya za display)

 "Serengeti" - 2026-11-14
Toleo la usalama na uimara. Maelezo kamili: [docs/SECURITY.md](docs/SECURITY.md).

### Usalama
- Kurasa za kernel ni supervisor-only; `PG_USER` inaruhusiwa tu ndani ya user space (`0x40000000-0x80000000`)
- Address space tofauti (page directory) kwa kila process; CR3 na `TSS.esp0` vinabadilishwa kwa kila task
- CR0.WP, NULL guard na guard page juu ya user stack
- Syscalls zinakagua pointer za user (`-EFAULT`), namba isiyojulikana inarudisha `-ENOSYS`
- Page Fault/GPF ya user inaua process hiyo tu kwa ujumbe unaoeleza kosa
- PMM: bitmap ya frames zilizofungwa (BIOS, kernel, boot data, page tables, heap); `pmm_free` inakagua alignment, mipaka, lock na double free
- Heap: ukaguzi wa overflow ya ukubwa, header yenye magic mbili, canary, poison `0xDD`, pointer validation kabla ya kusoma header, `heap_check`
- Mpangilio wa kuwaka `pmm_init` → `paging_init` → `heap_init`; heap ni eneo linalofuatana lililofungwa, juu ya kernel na ndani ya ramani
- Kiwango cha chini cha RAM: 16 MiB; ramani ya RAM imewekewa kikomo cha 1 GiB

### Imeongezwa
- ELF32 loader yenye ukaguzi wa mipaka, programu 9 za user zilizoandikwa kwa C (`user/`)
- Amri mpya: `progs`, `run <prog> [&]`, `security`, `selftest`; Ctrl+C inaua process ya mbele
- `ps` inaonyesha MODE na USER-PAGES; `mem` inaonyesha heap, integrity na frees zilizokataliwa
- Idle task
- Majaribio: `selftest` 23, `security` 6, `make test` checks 26

### Imerekebishwa
- VKFS: superblock na directory entries zinathibitishwa wakati wa mount; makosa ya I/O yanashughulikiwa
- ATA: ukaguzi wa mipaka ya LBA na kikomo cha LBA28
- VukangDB: schema inathibitishwa inapopakiwa, rows zinahesabiwa upya, TEXT inamalizwa kwa `\0`, kushindwa kukuza table kunashughulikiwa
- Ukaguzi wa `NULL` baada ya `kmalloc` kwenye shell na DB
- `sleep` wakati tasks zote zimelala

### Imeondolewa
- `kernel/arch/usermode.asm` (programu za user sasa ni ELF halisi)

## [0.1.0] "Kilimanjaro" - 2026-11-14
### Imeongezwa
- Multiboot boot, GDT + TSS, IDT, exceptions, PIC, PIT 100 Hz
- Physical frame allocator, paging ya 4 KiB, kernel heap ya 8 MiB
- Preemptive multitasking (tasks 32), sleep, kill, daemon ya saa
- System calls `int 0x80`, programu za ring 3, ulinzi wa GPF
- Drivers: VGA, serial, keyboard, RTC, ATA PIO, power, PC speaker
- VKFS filesystem (disk au RAM disk)
- VukangDB: CREATE/DROP/INSERT/SELECT/UPDATE/DELETE, WHERE na AND, ORDER BY, LIMIT, aggregates, PRIMARY KEY, BEGIN/COMMIT/ROLLBACK
- VKSH shell yenye amri 25 na `demo`
- Majaribio ya kiotomatiki (QEMU + serial) na GitHub Actions CI
