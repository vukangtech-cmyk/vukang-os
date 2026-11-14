# Usanifu wa VUKANG OS

Hati hii inaeleza kila sehemu ya OS, kwa mpangilio ambao kompyuta inavipitia inapowaka.

## Picha kubwa

```
+---------------------------------------------------------------+
|  VKSH shell  |  VukangDB (SQL)  |  programu za ring 3 (user)  |
+---------------------------------------------------------------+
|        system calls (int 0x80)    |    VKFS filesystem         |
+---------------------------------------------------------------+
|  scheduler/tasks | heap | paging | frame allocator | blockdev  |
+---------------------------------------------------------------+
|  GDT/TSS | IDT/ISR | PIC | PIT | keyboard | VGA | serial | ATA |
+---------------------------------------------------------------+
|                   Hardware (x86 CPU, RAM, disk)               |
+---------------------------------------------------------------+
```

## 1. Kuwaka (boot): `boot/boot.asm`

1. BIOS inapakia GRUB, ambayo inasoma `grub.cfg`.
2. GRUB inatafuta **Multiboot header** (namba ya kichawi `0x1BADB002`) ndani ya kernel yetu.
3. GRUB inaweka kernel kwenye anwani ya 1 MiB, inawasha protected mode ya 32-bit, kisha inaruka kwenda `_start`.
4. `_start` inaweka stack ya 32 KiB na kuita `kmain(magic, multiboot_info)` iliyoandikwa kwa C.

`linker.ld` ndiyo inayoamua kernel ikae wapi kwenye kumbukumbu, na inatoa alama `kernel_start` na `kernel_end` ambazo memory manager inazitumia.

## 2. CPU: `kernel/arch/`

- **GDT (`gdt.c`)**: Inafafanua segments 6: null, kernel code/data (ring 0), user code/data (ring 3) na TSS. Hii ndiyo msingi wa kutenganisha kernel na programu za mtumiaji.
- **TSS**: Inaiambia CPU itumie stack gani inaporudi kutoka ring 3 kwenda ring 0.
- **IDT (`idt.c`) + `isr.asm`**: Jedwali la vector 256. Vector 0–31 ni makosa ya CPU (kugawanya kwa sifuri, page fault, GPF...). 32–47 ni IRQ za hardware. 0x80 ni syscall.
- **PIC**: Tunahamisha IRQ za hardware kwenda vector 32–47 ili zisigongane na exceptions za CPU.
- **Timer (`timer.c`)**: PIT inapiga mara 100 kwa sekunde. Kila "tick" inaendesha scheduler.

### Ubunifu muhimu: kubadilisha task kwa kurudisha stack pointer

Kila interrupt inapita `isr_common` (assembly). Inahifadhi register zote, kisha inaita `interrupt_dispatch()` ya C. Dispatcher hiyo **inarudisha stack pointer**. Kama scheduler ikirudisha stack ya task nyingine, assembly inapakia register za task hiyo na `iret` inaingia ndani yake. Hivyo context switch nzima ni mstari mmoja tu: `mov esp, eax`.

## 3. Kumbukumbu: `kernel/mm/`

Mpangilio wa kuwaka ni **`pmm_init` → `paging_init` → `heap_init`**, ili heap ijengwe juu ya RAM halisi iliyokwisha wekwa kwenye page tables.

### Ramani ya kumbukumbu (virtual)

```
0x00000000          ukurasa 0: HAUJAWEKWA (NULL guard)
0x00001000 .. 1 MiB BIOS, VGA 0xB8000          supervisor-only, locked
0x00100000 ..       kernel image (.text/.data/.bss)  supervisor-only, locked
   ..               heap ya kernel (8 MiB)     supervisor-only, locked
   .. kernel_limit  RAM iliyobaki (frames huru) supervisor-only
0x40000000          USER_BASE: code + data ya ELF (ya kila process peke yake, PG_USER)
   ..               nafasi huru ya user
0x7FFFB000..7FFFF000 user stack (kurasa 4)
0x7FFFF000          guard page (haijawekwa)
0x80000000          USER_END
```

- **Frame allocator (`pmm.c`)**: Bitmap mbili. `used[]` inaonyesha frame inayotumika, `locked[]` inaonyesha frame ambayo haiwezi kuachiliwa kamwe: BIOS, kernel image, data ya multiboot (info, mmap, cmdline, modules), page tables za kernel na heap. Ramani ya GRUB inasomwa kwa namba za 64-bit na kuwekewa kikomo cha 1 GiB. `pmm_free` inakagua alignment, mipaka, lock na double free kabla ya kugusa hesabu, na inarudisha `pmm_status_t`. Pia kuna `pmm_alloc_zero` na `pmm_alloc_contiguous`.
- **Paging (`paging.c`)**: Page directory + page tables (ngazi 2). RAM inayosimamiwa inawekwa identity map **bila `PG_USER`**, hivyo ring 3 haiwezi kuigusa. Ukurasa 0 haujawekwa (NULL pointer = page fault) na CR0.WP imewashwa. Kila process ina page directory yake inayoshiriki PDE za kernel, lakini sehemu ya user ni yake peke yake. API: `vmm_create_space`, `vmm_destroy_space`, `vmm_map` (inakataa `PG_USER` nje ya user range), `vmm_lookup`, `vmm_switch`, `vmm_user_range_ok`, `vmm_user_strlen`.
- **Heap (`heap.c`)**: `kmalloc`/`kfree`/`krealloc` juu ya eneo linalofuatana lililoombwa kutoka PMM na kufungwa (8 MiB, au nusu yake hadi 1 MiB kama RAM ni ndogo). Inathibitishwa kuwa juu ya `kernel_end` na ndani ya ramani ya kernel. Kila block ina header ya bytes 16 (magic `VKHU`/`VKHF`, size, req, next), canary `0xC0FFEE42` baada ya data, na kumbukumbu inayoachiliwa inajazwa `0xDD`. `kfree` inathibitisha pointer kabla ya kusoma header. Maelezo kamili yako [SECURITY.md](SECURITY.md).

## 4. Processes na user mode: `kernel/proc/`, `user/`

- **`task.c`**: Hadi tasks 32. Kila task ina kernel stack ya 16 KiB, na process ya user pia ina page directory (`pd`). Kuna **idle task** (`sti; hlt`) ili tasks zikilala zote CPU ipumzike. Scheduler (round-robin) inabadilisha CR3 na `TSS.esp0` kwa kila task. `process_spawn` inaunda address space, inapakia ELF, inaweka user stack na kuchora interrupt frame ya ring 3 ambayo `iret` inaiingia. `task_wait`, `task_try_wait` na `task_detach` zinasubiri au kuachia process; ikifa, frames zake zote zinarudishwa PMM.
- **`elf.c`**: ELF32 loader yenye ukaguzi wa mipaka: header, aina (i386, executable), kila segment lazima iwe ndani ya user space na chini ya stack, na data isizidi faili.
- **`programs.c` + `user/`**: Programu za user zimeandikwa kwa C (`user/*.c`, `ulib.h`, `crt0.c`) na kuunganishwa kwenye `0x40000000` kwa `user/user.ld`. `Makefile` inazijenga kama ELF na kuziweka ndani ya kernel kwa `incbin` (`build/user/programs.asm`).
- **`syscall.c`**: `int 0x80` yenye `EAX` = namba ya syscall:

| # | Syscall | Kazi |
|---|---|---|
| 0 | exit(code) | maliza process |
| 1 | write(str) | andika kwenye console (pointer inakaguliwa, `-EFAULT` = -14) |
| 2 | ticks() | muda tangu kuwaka |
| 3 | yield() | toa CPU kwa task nyingine |
| 4 | getpid() | namba ya task |
| 5 | sleep(ms) | lala |

Namba isiyojulikana inarudisha `-ENOSYS`.

- **Makosa (`kernel/arch/idt.c`)**: Page Fault au GPF kutoka ring 3 inaua process hiyo tu (exit -11 / -13) kwa ujumbe unaoeleza kosa. Kosa ndani ya kernel bado ni panic.

## 5. Drivers: `drivers/`

| Driver | Jinsi inavyofanya kazi |
|---|---|
| VGA | Inaandika moja kwa moja kwenye kumbukumbu `0xB8000` (80x25, rangi 16), ikiwa na scrolling na cursor |
| Serial COM1 | UART 16550. Kila maandishi yanaenda pia serial, hivyo OS inaweza kuendeshwa bila skrini na kupimwa kiotomatiki |
| Keyboard | IRQ1, scancode set 1, Shift/Caps/Ctrl, ring buffer |
| RTC | Inasoma saa na tarehe kutoka CMOS (BCD) |
| ATA | PIO LBA28, IDENTIFY, kusoma na kuandika sectors za 512 B |
| Power | Reboot kupitia 8042, shutdown kupitia ACPI port (QEMU/Bochs/VirtualBox) |

## 6. Hifadhi: `fs/`

- **`blockdev.c`**: Kiolesura kimoja cha "block device". Kama hard disk ipo inatumika ATA. Kama haipo, kuna RAM disk ya 4 MiB. Sehemu za juu hazijui tofauti.
- **VKFS (`vkfs.c`)**: Wakati wa mount superblock inakaguliwa (geometry na mipaka), na kila entry ya directory inathibitishwa; entries mbovu zinaondolewa. Muundo wa disk:

```
LBA 0      : imehifadhiwa (boot sector)
LBA 1      : superblock (magic "VKFS", ukubwa, mahali pa bitmap/directory)
LBA 2..    : bitmap ya nafasi huru (bit 1 kwa kila sector)
sectors 8  : directory (faili 64 x bytes 64: jina, ukubwa, mwanzo, muda)
zinazobaki : data; kila faili ni kipande kimoja kinachofuatana (extent)
```

Faili linapokua, linapewa nafasi ya ziada ya 50% ili lisihamishwe kila mara.

## 7. VukangDB: `db/`

Database ndani ya OS. Ina tabaka tatu:

1. **Lexer (`lexer.c`)**: Inavunja SQL kuwa tokens (maneno, namba, maandishi, alama kama `>=` na `!=`).
2. **Storage engine (`storage.c`)**: Kila table ni faili `jina.tbl` ndani ya VKFS. Faili lina header ya bytes 512 (schema: columns hadi 8, aina INT/TEXT, PRIMARY KEY) ikifuatiwa na rows za ukubwa sawa. Tables zinakaa kwenye **cache (buffer pool)** ya tables 16.
3. **SQL engine (`engine.c`)**: Parser ya recursive-descent inayotekeleza amri moja kwa moja.

**Transactions**: Bila `BEGIN`, kila mabadiliko yanaandikwa disk papo hapo (auto-commit). Ndani ya `BEGIN`, mabadiliko yanakaa kwenye cache tu. `COMMIT` inayaandika disk, na `ROLLBACK` inatupa tables chafu (dirty) kutoka cache. Hivyo data ya disk inabaki kama ilivyokuwa.

**Mfano wa safari ya query** `SELECT jina FROM wanafunzi WHERE alama > 80 ORDER BY alama DESC`:
lexer → parser inatambua SELECT → `tbl_open` inapakia table (kutoka cache au VKFS → ATA) → kila row inapimwa na WHERE → rows zinapangwa (insertion sort) → jedwali linachorwa kwenye skrini.

## 7b. Skrini na boot animation: `drivers/fb.c`, `drivers/vga.c`, `kernel/splash.c`

- **Ombi la video:** Multiboot header (`boot/boot.asm`) ina bit 2 ya flags na inaomba 1024x768x32. GRUB inaweka framebuffer na kutupa anwani yake ndani ya `multiboot_info`.
- **`fb.c`:** `fb_probe` inakubali framebuffer ya RGB tu (bpp 16/24/32) yenye vipimo vya kweli. Kama iko nje ya ramani ya kernel inawekwa kwenye `FB_VIRT = 0xC0000000` kama kurasa za **supervisor-only** (write-through); vinginevyo frames zake zinafungwa kwenye PMM. `vmm_create_space` inanakili PDE za juu ya `USER_END`, kwa hiyo kila process inaiona kernel (na framebuffer) lakini haiwezi kuigusa kutoka ring 3.
- **`vga.c`:** console yenye backend mbili. Herufi zote zinakaa kwenye **shadow cells**; backend ya text inaandika `0xB8000`, backend ya framebuffer inachora kila herufi kwa font ya 10x20 (Terminus Bold, `assets/font/`). Damage tracking (`shown[]`) inachora upya seli zilizobadilika tu, kwa hiyo scroll na saa ni za haraka. Palette ya rangi 16 ni ya kisasa (mandharinyuma `0x080B10`, cyan `0x3FE0F0`). `vga_hold()` inazuia kuchora wakati animation inaendelea, kisha `vga_redraw()` inaonyesha log nzima ya kuwaka.
- **`splash.c`:** back buffer (karibu MB 1.3 kwenye heap) inachorwa kila frame kisha inanakiliwa kwenye framebuffer. Tabaka: chembe 56 za mwanga zinazopanda, nembo ya "V" inayokuza kutoka 0.81 hadi 1.0 na kung'aa kama inapumua, jina "VUKANG" linalopanda na kuonekana polepole, progress bar ya gradient yenye mwanga unaosogea, maandishi ya hatua, mstari wa toleo, mwanga unaopita juu ya nembo, na fade-out. Progress inafuata hatua halisi za `kmain` (`splash_step`): 40 kernel, 60 storage, 80 VKFS, 95 VukangDB. Kitufe chochote kinaruka animation, `panic` inaita `splash_abort`, na bila framebuffer kuna toleo la text (jina + bar ya blocks).
- **Assets:** `kernel/assets.asm` inaweka `console.vkfont`, `emblem.vkimg` na `wordmark.vkimg` ndani ya kernel kwa `incbin`. Muundo wa `.vkimg`: `"VKIM"`, upana na urefu (u16), kisha pixels za RGB.

## 8. Shell: `shell/shell.c`

Jedwali la amri (`commands[]`): kila amri ina jina, maelezo na function. Kuongeza amri mpya ni kuongeza mstari mmoja tu. `clockd` ni task ya nyuma (daemon) inayoonyesha saa kona ya juu kulia. Hiyo ni ushahidi wa multitasking unaoonekana kwa macho.

## 9. Majaribio: `kernel/selftest.c`, `scripts/qemu_test.py`

- `selftest`: majaribio 23 ndani ya kernel (PMM, heap, paging).
- `security`: programu 6 za ring 3 zinazoshambulia kernel.
- `make test`: checks 30 kupitia serial console ya QEMU, pamoja na leak ya frames, persistence ya disk, directory entry mbovu na superblock ya uongo.

## Mipaka ya sasa (kwa uwazi)

- 32-bit, CPU moja (hakuna SMP bado).
- Programu za ELF zimewekwa ndani ya kernel image. Kuzipakia kutoka VKFS, `fork`/`exec` na syscalls za faili ni kazi ya v0.3.
- Hakuna NX bit (inahitaji PAE) wala ASLR.
- VukangDB: aina INT na TEXT (herufi 31), columns 8, hakuna JOIN wala index (B-tree) bado, na hakuna journal ya kulinda data umeme ukikatika katikati ya kuandika.

Haya yote yako kwenye [ROADMAP](ROADMAP.md).
