# Changelog

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
