# Roadmap ya VUKANG OS

## v0.1.0 "Kilimanjaro" (imekamilika)
Kernel, GDT/IDT/TSS, paging, heap, scheduler, syscalls, ring 3, drivers 7, VKFS, VukangDB (SQL + transactions), shell, CI.

## v0.2.0 "Serengeti": Usalama na uimara (imekamilika)
- [x] Kurasa za kernel supervisor-only, `PG_USER` kwa user pages tu
- [x] Address space tofauti kwa kila process (page directory ya kila mmoja)
- [x] ELF loader (programu zilizowekwa ndani ya kernel)
- [x] PMM yenye ukaguzi wa `free` na frames zilizofungwa
- [x] Heap yenye ukaguzi wa overflow, canary na pointer validation
- [x] Mpangilio salama wa kuwaka na ukaguzi wa heap dhidi ya RAM na ramani
- [x] Ukaguzi wa VKFS, ATA na VukangDB dhidi ya data iliyoharibika
- [x] `security`, `selftest`, checks 30 za CI
- [x] v0.2.1: boot animation na console ya graphics (framebuffer)
- [x] v0.2.2: usalama wa data: disk ikijaa, INSERT/UPDATE za yote au hakuna, PRIMARY KEY kwenye UPDATE (checks 42)

## v0.3.0 "Ngorongoro": Programu halisi na database yenye nguvu
- [ ] ELF loader: programu za nje kutoka VKFS
- [ ] `fork` / `exec` / `wait`
- [ ] libc ndogo + syscalls za faili (open/read/write/close)
- [ ] Folda (directories) kwenye VKFS
- [ ] B-tree index kwa PRIMARY KEY (utafutaji wa O(log n))
- [ ] JOIN ya tables mbili
- [x] Write-Ahead Log (WAL): data salama hata umeme ukikatika (`db/wal.c`, `docs/WAL.md`, checks 41)
- [ ] Aina mpya: FLOAT, DATE, TEXT ndefu
- [ ] Historia ya amri kwa vitufe vya mshale (shell)

## v0.4.0 "Meru": Mtandao na picha
- [ ] Driver ya mtandao (RTL8139 / e1000) + TCP/IP ndogo
- [ ] VukangDB inapatikana kupitia mtandao (client/server)
- [ ] Graphics mode (VESA framebuffer), fonti na madirisha rahisi
- [ ] Mouse

## v1.0.0 "Uhuru"
- [ ] PAE + NX bit, ASLR
- [ ] 64-bit (x86_64) na UEFI boot
- [ ] Multicore (SMP)
- [ ] Toleo la appliance kwa POS/kiosk
- [ ] Kozi kamili ya kufundishia: "Jenga OS yako" (Kiswahili + Kiingereza)
