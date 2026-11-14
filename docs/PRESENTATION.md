# Mwongozo wa Innovation Presentation: VUKANG OS

Lengo ni presentation ya dakika 10 hadi 12: slides 8, demo ya moja kwa moja ya dakika 4, kisha maswali.

## Ujumbe mkuu (sentensi moja)

> "VUKANG OS ni mfumo wa uendeshaji uliojengwa Tanzania kuanzia sifuri, kwa C na Assembly, na ndani yake una database ya SQL iliyojengwa moja kwa moja kwenye OS."

## Takwimu za kutaja

- Takriban mistari 3,200 ya C na Assembly, yote imeandikwa na sisi.
- Sifuri maktaba za nje: hakuna Linux, hakuna libc.
- Subsystems 11: boot, CPU, memory, paging, heap, multitasking, processes (ELF + address space), syscalls, drivers 7, filesystem, database.
- Mashambulizi 6 kutoka ring 3, yote yanazuiwa (`security`: "6/6 protections held").
- Majaribio 23 ndani ya kernel (`selftest`) na checks 30 za kiotomatiki ndani ya QEMU kwenye kila PR.
- Boot animation ya fps karibu 100 inayochorwa na kernel yenyewe, bila GPU driver wala maktaba ya graphics.
- ISO ya MB 5.6 inayowaka kwa chini ya sekunde 3.

## Muundo wa slides

| # | Kichwa | Maudhui | Muda |
|---|---|---|---|
| 1 | VUKANG OS | Washa OS ili majaji waone boot animation (au `docs/images/00-boot-animation.gif`), kisha banner (`docs/images/01-boot.png`), jina lako, "Made in Tanzania" | 0:30 |
| 2 | Tatizo | Afrika inatumia teknolojia ya msingi ya nje. Wanafunzi wanajifunza OS kwa nadharia tu. Mifumo midogo (POS, kliniki, shule) inabeba OS nzito pamoja na database tofauti | 1:00 |
| 3 | Suluhisho | OS nyepesi ambayo database ni sehemu ya mfumo wenyewe, si programu ya ziada | 1:00 |
| 4 | Usanifu | Mchoro wa tabaka kutoka `ARCHITECTURE.md` (hardware → kernel → VKFS → VukangDB → shell) | 1:30 |
| 5 | Ubunifu (innovation) | Ona sehemu ya "Hoja za ubunifu" hapa chini | 1:30 |
| 6 | DEMO YA MOJA KWA MOJA | Endesha `demo` | 4:00 |
| 7 | Matumizi na soko | Elimu (kufundishia OS na DB vyuoni), embedded/kiosk, POS za maduka, rekodi za kliniki zisizohitaji intaneti | 1:00 |
| 8 | Roadmap na ombi | v0.3 hadi v1.0 kutoka `ROADMAP.md`. Unachoomba: mentorship, hardware ya kupima, ufadhili | 1:00 |

## Hoja za ubunifu

1. **Database ndani ya OS.** Kwa kawaida DBMS ni programu inayokaa juu ya OS. VukangDB iko ndani ya mfumo na inaongea na filesystem moja kwa moja. Faida ni kwamba hakuna kitu cha ziada cha kusakinisha, kuna tabaka chache, na data iko tayari tangu sekunde ya kwanza.
2. **Kila kitu kimejengwa nyumbani.** Kuanzia instruction ya kwanza ya CPU hadi `SELECT`. Hii inaonyesha uwezo wa uhandisi wa kina (deep tech) hapa Tanzania.
3. **Usalama halisi wa vifaa.** Ring 0 na ring 3, kurasa za kernel ni supervisor-only, na kila programu ina address space yake. Programu mbaya inazuiwa na CPU yenyewe (MMU), inauawa peke yake, na OS inaendelea kufanya kazi. Hii ni dhana ile ile ya Linux na Windows.
4. **Kernel inayojilinda.** Memory manager inakataa free mbaya, double free, overflow ya ukubwa na buffer overflow (canary), na inaweza kujipima yenyewe (`selftest`).
5. **Transactions.** `BEGIN` / `ROLLBACK` zinalinda data, dhana ile ile inayotumika na benki.
6. **Inaweza kupimwa kiotomatiki.** Serial console inaruhusu OS nzima kupimwa bila binadamu. Hii ni nidhamu ya kitaalamu ya uhandisi.

## Demo script (dakika 4)

Andaa kabla: `make iso`, futa disk (`rm build/disk.img`), kisha `make run`. Ongeza ukubwa wa dirisha la QEMU.

| Hatua | Andika | Sema |
|---|---|---|
| 1 | (boot) | "Hii ni OS inayowaka. Kila mstari wa [ OK ] ni subsystem tuliyoiandika wenyewe." |
| 2 | `about` | "Hakuna Linux chini yake. Ni kernel yetu, memory manager yetu." |
| 3 | `demo` | Endesha showcase yote. Ielezee inapopita: faili, SQL, transaction, tasks, user mode. |
| 4 | `sql BEGIN; DELETE FROM wanafunzi` kisha `sql ROLLBACK; SELECT * FROM wanafunzi` | "Nimefuta kila kitu kimakosa, lakini ROLLBACK imerudisha data yote." |
| 5 | `reboot` kisha `sql SELECT * FROM wanafunzi` | "Data imebaki baada ya kuzima. Imeandikwa kwenye hard disk na VKFS." |
| 6 | Onyesha saa kona ya juu | "Saa hiyo ni task inayokimbia sambamba na shell. Ndiyo multitasking." |
| 7 | `run hello` | "Hii ni programu ya ELF inayokimbia ring 3 kwenye address space yake. Code iko 0x40000000, stack iko 0x7FFFEFDC." |
| 8 | `security` | "Programu 6 zinajaribu kusoma kernel, kuiandika, kuiendesha, kugusa hardware, kuzima interrupts na kuidanganya kupitia syscall. Zote zinazuiwa na kernel bado inaendelea." |
| 9 | `selftest` kisha `mem` | "Kernel inajipima yenyewe: majaribio 23 ya memory, yote PASS. `mem` inaonyesha frees mbaya zilizokataliwa." |

Picha za akiba: `docs/images/05-security.png` na `docs/images/06-selftest.png`.

**Mpango wa dharura:** Kama laptop au projector itakataa, tumia picha zilizo `docs/images/` na rekodi ya video ya demo (rekodi kabla ya siku ya tukio, kwa mfano kwa OBS).

## Maswali yanayotarajiwa kutoka kwa majaji

**"Kwa nini usitumie Linux tu?"**
Linux ni nzuri, lakini lengo letu ni kumiliki na kuelewa kila tabaka, na kuunda muundo mpya ambapo database ni sehemu ya OS. Pia ni jukwaa la kufundishia: kila mstari unaeleweka.

**"Inawaka kwenye kompyuta halisi?"**
Ndiyo, kwenye kompyuta za x86 zenye BIOS/Legacy boot, kupitia ISO kwenye USB. Pia inawaka kwenye QEMU na VirtualBox. UEFI na 64-bit viko kwenye roadmap.

**"Ni salama?"**
Katika v0.2 tulifanya ukaguzi wa usalama na kurekebisha matatizo 4 makubwa: kurasa za kernel sasa ni supervisor-only, kila process ina address space yake, frame allocator inakataa free mbaya, na heap inakagua overflow na pointer. Tunaonyesha hili moja kwa moja kwa `security` (mashambulizi 6, yote yanazuiwa). Hatua zinazofuata ni NX bit na ASLR. Maelezo yako `docs/SECURITY.md`.

**"Mlijuaje kwamba kuna tatizo la usalama?"**
Kwa kukagua msimbo (code review): tuligundua kwamba `PG_USER` ilikuwa kwenye kila ukurasa. CPU ilizuia `cli` lakini programu ya user ingeweza kusoma kernel. Tuliandika programu ya shambulizi kwanza, tukaona inafanikiwa, kisha tukarekebisha na sasa inazuiwa. Jaribio hilo linabaki kwenye CI milele.

**"Mtindo wa biashara?"**
Kwanza elimu: kozi na maabara ya OS na DB kwa vyuo vya Afrika Mashariki. Pili, OS maalum (appliance) kwa POS na kiosk zisizohitaji intaneti. Leseni ni open source (MIT), na mapato yanatoka kwenye mafunzo, msaada wa kiufundi na matoleo maalum.

**"Ni nini kigumu zaidi ulichokutana nacho?"**
Kubadilisha task (context switching) na kushuka kwenda ring 3. Hapo kosa moja la byte moja linazima kompyuta nzima bila ujumbe wowote. Tulijenga serial console na majaribio ya kiotomatiki ili kuyagundua makosa hayo.

## Vidokezo

- Usisome slides. Demo ndiyo nyota ya presentation.
- Sema namba: "mistari 3,200", "mashambulizi 6 yote yamezuiwa", "majaribio 23 + 30", "sekunde 3 kuwaka".
- Kuwa mkweli kuhusu mipaka (32-bit, hakuna JOIN bado). Majaji wanaheshimu uwazi na roadmap iliyo wazi.
