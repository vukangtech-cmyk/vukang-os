# Roadmap ya VUKANG OS

## v0.1.0 "Kilimanjaro" (imekamilika)
Kernel, GDT/IDT/TSS, paging, heap, scheduler, syscalls, ring 3, drivers 7, VKFS, VukangDB (SQL + transactions), shell, CI.

## v0.2.0 "Serengeti": Database yenye nguvu zaidi
- [ ] B-tree index kwa PRIMARY KEY (utafutaji wa O(log n))
- [ ] JOIN ya tables mbili
- [ ] Write-Ahead Log (WAL): data salama hata umeme ukikatika
- [ ] Aina mpya: FLOAT, DATE, TEXT ndefu
- [ ] Historia ya amri kwa vitufe vya mshale (shell)

## v0.3.0 "Ngorongoro": Programu halisi
- [ ] ELF loader: programu za nje kutoka VKFS
- [ ] Address space tofauti kwa kila process (page directory ya kila mmoja)
- [ ] libc ndogo + syscalls za faili (open/read/write/close)
- [ ] `fork` / `exec` / `wait`
- [ ] Folda (directories) kwenye VKFS

## v0.4.0 "Meru": Mtandao na picha
- [ ] Driver ya mtandao (RTL8139 / e1000) + TCP/IP ndogo
- [ ] VukangDB inapatikana kupitia mtandao (client/server)
- [ ] Graphics mode (VESA framebuffer), fonti na madirisha rahisi
- [ ] Mouse

## v1.0.0 "Uhuru"
- [ ] 64-bit (x86_64) na UEFI boot
- [ ] Multicore (SMP)
- [ ] Toleo la appliance kwa POS/kiosk
- [ ] Kozi kamili ya kufundishia: "Jenga OS yako" (Kiswahili + Kiingereza)
