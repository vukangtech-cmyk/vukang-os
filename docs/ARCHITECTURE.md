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

- **Frame allocator (`pmm.c`)**: GRUB inatupa ramani ya RAM. Tunatumia bitmap yenye bit 1 kwa kila ukurasa wa 4 KiB (0 = huru, 1 = unatumika). Eneo la BIOS na kernel yenyewe vinahifadhiwa.
- **Paging (`paging.c`)**: Page directory + page tables (ngazi 2). RAM yote inawekwa identity map (virtual = physical). **Ukurasa 0 hauwekwi**, hivyo kosa la NULL pointer linasababisha page fault badala ya kuharibu data kimya kimya.
- **Heap (`heap.c`)**: `kmalloc`/`kfree` ya 8 MiB yenye first-fit, kugawa blocks (split) na kuunganisha blocks huru (coalesce). Kila block ina namba ya kichawi `VKHP`, hivyo uharibifu wa heap au double free unagunduliwa mara moja.

## 4. Tasks na user mode: `kernel/proc/`

- **`task.c`**: Hadi tasks 32. Kila task ina stack ya 16 KiB. Task mpya inaundwa kwa "kuchora" interrupt frame ya bandia juu ya stack yake, ili scheduler iweze kuiingia kama ilivyokatizwa tu. Hali za task ni ready, running, sleeping na dead.
- **`syscall.c`**: `int 0x80` yenye `EAX` = namba ya syscall:

| # | Syscall | Kazi |
|---|---|---|
| 0 | exit(code) | maliza programu |
| 1 | write(str) | andika kwenye console |
| 2 | ticks() | muda tangu kuwaka |
| 3 | yield() | toa CPU kwa task nyingine |
| 4 | getpid() | namba ya task |
| 5 | sleep(ms) | lala |

- **`usermode.asm`**: `enter_usermode` inashusha CPU kwenda ring 3 kwa `iret`. Programu ya ring 3 ikijaribu amri ya hatari (mfano `cli`), CPU inatoa General Protection Fault. Kernel inaimaliza programu hiyo tu na kuendelea kufanya kazi. Hiki ndicho kiini cha usalama wa OS za kisasa.

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
- **VKFS (`vkfs.c`)**: Muundo wa disk:

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

## 8. Shell: `shell/shell.c`

Jedwali la amri (`commands[]`): kila amri ina jina, maelezo na function. Kuongeza amri mpya ni kuongeza mstari mmoja tu. `clockd` ni task ya nyuma (daemon) inayoonyesha saa kona ya juu kulia. Hiyo ni ushahidi wa multitasking unaoonekana kwa macho.

## Mipaka ya sasa (kwa uwazi)

- 32-bit, CPU moja (hakuna SMP bado).
- Programu za user zinaishi ndani ya kernel image; bado hakuna ELF loader wala address space tofauti kwa kila programu.
- VukangDB: aina INT na TEXT (herufi 31), columns 8, hakuna JOIN wala index (B-tree) bado, na hakuna journal ya kulinda data umeme ukikatika katikati ya kuandika.

Haya yote yako kwenye [ROADMAP](ROADMAP.md).
