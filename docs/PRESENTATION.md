# Mwongozo wa Innovation Presentation: VUKANG OS

Lengo ni presentation ya dakika 10 hadi 12: slides 8, demo ya moja kwa moja ya dakika 4, kisha maswali.

## Ujumbe mkuu (sentensi moja)

> "VUKANG OS ni mfumo wa uendeshaji uliojengwa Tanzania kuanzia sifuri, kwa C na Assembly, na ndani yake una database ya SQL iliyojengwa moja kwa moja kwenye OS."

## Takwimu za kutaja

- Takriban mistari 2,500 ya C na Assembly, yote imeandikwa na sisi.
- Sifuri maktaba za nje: hakuna Linux, hakuna libc.
- Subsystems 10: boot, CPU, memory, paging, heap, multitasking, syscalls/user mode, drivers 7, filesystem, database.
- Majaribio 16 ya kiotomatiki yanayowasha OS halisi ndani ya QEMU, kuanzia kuwaka hadi data kubaki baada ya reboot.
- ISO ya MB 5.6 inayowaka kwa chini ya sekunde 3.

## Muundo wa slides

| # | Kichwa | Maudhui | Muda |
|---|---|---|---|
| 1 | VUKANG OS | Logo/banner (`docs/images/01-boot.png`), jina lako, "Made in Tanzania" | 0:30 |
| 2 | Tatizo | Afrika inatumia teknolojia ya msingi ya nje. Wanafunzi wanajifunza OS kwa nadharia tu. Mifumo midogo (POS, kliniki, shule) inabeba OS nzito pamoja na database tofauti | 1:00 |
| 3 | Suluhisho | OS nyepesi ambayo database ni sehemu ya mfumo wenyewe, si programu ya ziada | 1:00 |
| 4 | Usanifu | Mchoro wa tabaka kutoka `ARCHITECTURE.md` (hardware → kernel → VKFS → VukangDB → shell) | 1:30 |
| 5 | Ubunifu (innovation) | Ona sehemu ya "Hoja za ubunifu" hapa chini | 1:30 |
| 6 | DEMO YA MOJA KWA MOJA | Endesha `demo` | 4:00 |
| 7 | Matumizi na soko | Elimu (kufundishia OS na DB vyuoni), embedded/kiosk, POS za maduka, rekodi za kliniki zisizohitaji intaneti | 1:00 |
| 8 | Roadmap na ombi | v0.2 hadi v1.0 kutoka `ROADMAP.md`. Unachoomba: mentorship, hardware ya kupima, ufadhili | 1:00 |

## Hoja za ubunifu

1. **Database ndani ya OS.** Kwa kawaida DBMS ni programu inayokaa juu ya OS. VukangDB iko ndani ya mfumo na inaongea na filesystem moja kwa moja. Faida ni kwamba hakuna kitu cha ziada cha kusakinisha, kuna tabaka chache, na data iko tayari tangu sekunde ya kwanza.
2. **Kila kitu kimejengwa nyumbani.** Kuanzia instruction ya kwanza ya CPU hadi `SELECT`. Hii inaonyesha uwezo wa uhandisi wa kina (deep tech) hapa Tanzania.
3. **Usalama halisi wa vifaa.** Ring 0 na ring 3. Programu mbaya inazuiwa na CPU yenyewe, na OS inaendelea kufanya kazi.
4. **Transactions.** `BEGIN` / `ROLLBACK` zinalinda data, dhana ile ile inayotumika na benki.
5. **Inaweza kupimwa kiotomatiki.** Serial console inaruhusu OS nzima kupimwa bila binadamu. Hii ni nidhamu ya kitaalamu ya uhandisi.

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

**Mpango wa dharura:** Kama laptop au projector itakataa, tumia picha zilizo `docs/images/` na rekodi ya video ya demo (rekodi kabla ya siku ya tukio, kwa mfano kwa OBS).

## Maswali yanayotarajiwa kutoka kwa majaji

**"Kwa nini usitumie Linux tu?"**
Linux ni nzuri, lakini lengo letu ni kumiliki na kuelewa kila tabaka, na kuunda muundo mpya ambapo database ni sehemu ya OS. Pia ni jukwaa la kufundishia: kila mstari unaeleweka.

**"Inawaka kwenye kompyuta halisi?"**
Ndiyo, kwenye kompyuta za x86 zenye BIOS/Legacy boot, kupitia ISO kwenye USB. Pia inawaka kwenye QEMU na VirtualBox. UEFI na 64-bit viko kwenye roadmap.

**"Ni salama?"**
Ina utengano wa ring 0/ring 3, ulinzi wa NULL pointer kupitia paging, na ukaguzi wa heap corruption. Hatua inayofuata ni address space tofauti kwa kila programu.

**"Mtindo wa biashara?"**
Kwanza elimu: kozi na maabara ya OS na DB kwa vyuo vya Afrika Mashariki. Pili, OS maalum (appliance) kwa POS na kiosk zisizohitaji intaneti. Leseni ni open source (MIT), na mapato yanatoka kwenye mafunzo, msaada wa kiufundi na matoleo maalum.

**"Ni nini kigumu zaidi ulichokutana nacho?"**
Kubadilisha task (context switching) na kushuka kwenda ring 3. Hapo kosa moja la byte moja linazima kompyuta nzima bila ujumbe wowote. Tulijenga serial console na majaribio ya kiotomatiki ili kuyagundua makosa hayo.

## Vidokezo

- Usisome slides. Demo ndiyo nyota ya presentation.
- Sema namba: "mistari 2,500", "majaribio 16", "sekunde 3 kuwaka".
- Kuwa mkweli kuhusu mipaka (32-bit, hakuna JOIN bado). Majaji wanaheshimu uwazi na roadmap iliyo wazi.
