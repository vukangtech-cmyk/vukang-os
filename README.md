# VUKANG OS

**Mfumo wa uendeshaji (Operating System) halisi, uliojengwa kuanzia sifuri kwa C na Assembly, wenye database (SQL) iliyojengwa ndani yake.**
Imetengenezwa Tanzania na VukangTech.

![VUKANG OS ikiwaka](docs/images/01-boot.png)

VUKANG OS haitumii Linux, Windows wala maktaba (library) yoyote ya nje. Kila kitu ni chetu: bootloader entry, kernel, memory manager, scheduler, drivers, filesystem, database engine na shell. Inawaka kwenye kompyuta halisi ya x86 au kwenye emulator (QEMU, VirtualBox).

## Kuna nini ndani

| Sehemu | Maelezo |
|---|---|
| Kernel | Monolithic, 32-bit protected mode, inawaka kupitia GRUB (Multiboot) |
| CPU | GDT, TSS, IDT, exceptions 32, IRQ 16, PIC remap |
| Memory | Frame allocator yenye bitmap mbili (used + locked) na ukaguzi wa kila `free`, paging ya ngazi 2 ambapo kurasa za kernel ni **supervisor-only**, heap ya 8 MiB yenye ukaguzi wa overflow, canary na pointer validation |
| Processes | Kila programu ina **address space yake** (page directory yake), ELF loader, stack yenye guard page, `run`, `ps`, `kill`, Ctrl+C |
| Multitasking | Preemptive round-robin scheduler kwa 100 Hz, idle task, `spawn`, `sleep` |
| Usalama | Ring 0 / ring 3, kurasa za kernel hazifikiwi na user, syscalls zinakagua kila pointer (`-EFAULT`), programu mbaya inauawa peke yake. `security` inaendesha mashambulizi 6 |
| Drivers | VGA text, PS/2 keyboard, serial COM1, PIT timer, CMOS RTC (saa), ATA hard disk, PC speaker, reboot/shutdown |
| Filesystem | **VKFS**, filesystem yetu wenyewe kwenye hard disk au RAM disk |
| Database | **VukangDB**, SQL engine ndani ya OS: tables, PRIMARY KEY, WHERE, ORDER BY, LIMIT, COUNT/SUM/AVG/MIN/MAX, UPDATE, DELETE, transactions (BEGIN/COMMIT/ROLLBACK), data inabaki baada ya kuzima |
| Shell | **VKSH**: amri 29, pamoja na `demo`, `security` na `selftest` kwa presentation |

## Kuijenga na kuiendesha

Kwenye Ubuntu/Debian (au WSL kwenye Windows):

```bash
sudo apt install build-essential nasm qemu-system-x86 grub-pc-bin grub-common xorriso mtools
git clone https://github.com/vukangtech-cmyk/vukang-os.git
cd vukang-os
make run          # inafungua dirisha la QEMU
make run-serial   # bila dirisha, shell ndani ya terminal (toka: Ctrl+A kisha X)
make test         # majaribio ya kiotomatiki (26 checks)
```

Faili `build/vukang-os.iso` linaweza kuwashwa kwenye VirtualBox, au kuandikwa kwenye USB na kuwashwa kwenye kompyuta halisi yenye BIOS/Legacy boot.

## Jaribu hivi

```
vukang:~$ demo
vukang:~$ sql CREATE TABLE wanafunzi (id INT PRIMARY KEY, jina TEXT, alama INT)
vukang:~$ sql INSERT INTO wanafunzi VALUES (1,'Asha',88),(2,'Juma',75)
vukang:~$ sql SELECT * FROM wanafunzi ORDER BY alama DESC
vukang:~$ db            # SQL console kamili
vukang:~$ spawn 3       # tasks 3 zinakimbia pamoja
vukang:~$ progs         # programu za ELF zilizopo
vukang:~$ run hello     # process ya ring 3 yenye address space yake
vukang:~$ run spin &    # process ya nyuma; `kill <pid>` au Ctrl+C kwa iliyo mbele
vukang:~$ security      # mashambulizi 6 dhidi ya kernel, yote yanazuiwa
vukang:~$ selftest      # majaribio 23 ya memory ndani ya kernel
vukang:~$ mem           # RAM, heap, paging na frees zilizokataliwa
```

![VukangDB](docs/images/03-vukangdb.png)

![Security suite](docs/images/05-security.png)

## Muundo wa mradi

```
boot/        Multiboot entry (assembly)
kernel/      kmain, selftest, arch/ (GDT, IDT, ISR, timer), mm/ (pmm, paging, heap), proc/ (tasks, ELF loader, programs, syscalls)
user/        programu za ring 3 (hello, counter, attack_*, spin) + ulib + linker script
drivers/     vga, serial, keyboard, rtc, ata, power
fs/          blockdev + VKFS
db/          VukangDB: lexer, storage engine, SQL engine
shell/       VKSH shell
lib/         string + kprintf
include/     headers
docs/        nyaraka (Kiswahili)
scripts/     majaribio ya kiotomatiki
```

## Nyaraka

- [Usanifu (Architecture)](docs/ARCHITECTURE.md): jinsi kila sehemu inavyofanya kazi
- [Usalama (Security)](docs/SECURITY.md): matatizo ya v0.1 na jinsi v0.2 ilivyoyatatua
- [Mwongozo wa Presentation](docs/PRESENTATION.md): slides, demo script na maswali ya majaji
- [Maintenance](docs/MAINTENANCE.md): jinsi tunavyoendeleza mradi
- [Roadmap](docs/ROADMAP.md): matoleo yajayo
- [CHANGELOG](CHANGELOG.md)

## Leseni

MIT. Angalia [LICENSE](LICENSE).
