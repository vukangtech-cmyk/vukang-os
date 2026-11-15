# Write-Ahead Log (WAL) ya VukangDB

## Tatizo
VKFS inaandika faili la table sector kwa sector. Umeme ukikatika katikati, table inakuwa nusu ya zamani na nusu mpya. COMMIT ya tables mbili inaweza kuisha na moja mpya na nyingine ya zamani.

## Wazo
**Andika nia kwanza, tekeleza baadaye.** Kabla ya kugusa table yoyote, andika picha KAMILI za tables zote zinazobadilika kwenye faili moja (`.wal`) lenye CRC32. Kisha sasisha tables, kisha futa log.

```
1. jenga log     : kichwa + [jina, urefu, picha] kwa kila table
2. andika .wal   : VKFS inaandika data kwanza, directory entry mwisho; CRC inafunika kila kitu
                   ===> HAPA NDIPO COMMIT INAPOTOKEA (commit point)
3. tekeleza      : vkfs_write() kila picha kwenye faili lake    (inaweza kurudiwa)
4. checkpoint    : futa .wal
```

## Recovery (kila boot, `wal_recover` ndani ya `db_init`)
| Hali ya `.wal` | Maana | Hatua |
|---|---|---|
| haipo | hakuna kazi nusu | hakuna |
| ipo, CRC sahihi | COMMIT ilifanyika | rudia hatua 3 na 4 (redo) |
| ipo, CRC mbovu / haijakamilika | COMMIT haikuisha | itupe; tables hazijabadilika |

Redo ni salama kurudiwa kwa sababu inaandika picha nzima (idempotent).

## Ukaguzi wa nafasi
Log inahitaji nafasi ya picha zote, kisha tables zinahitaji nafasi yao. Kabla ya kuandika chochote, `wal_commit` inarudia kanuni halisi ya VKFS kwenye nakala ya metadata (`vkfs_sim_begin/end`, bila kuandika disk). Isipotosha, COMMIT inakataliwa kwa kosa lililo wazi, na hakuna kitu kinachoguswa. Kwa hiyo log iliyo halali haiwezi kushindwa kutekelezwa kwa sababu ya nafasi.

## Majaribio ya kukata umeme
`walcrash <n>` inasimamisha mashine papo hapo kwenye hatua n ya COMMIT inayofuata:

| n | Mahali | Baada ya kuwasha upya |
|---|---|---|
| 1 | kabla ya log | data ya zamani |
| 2 | log imehifadhiwa, hakuna table | data mpya (log inarudiwa) |
| 3 | log imehifadhiwa, table ya kwanza nusu | data mpya |
| 4 | tables zote zimeandikwa, log haijafutwa | data mpya |
| 5 | log yenyewe imeharibika | data ya zamani |

`make test-wal` inaendesha kila hatua kwa table moja na kwa tables mbili (lazima ziwe zote za zamani au zote mpya).

## Mipaka
- Log ina ukubwa wa juu wa 512 KiB; transaction kubwa zaidi inakataliwa.
- Gharama: kila COMMIT inaandika mara mbili (log kisha table), na inahitaji nafasi ya karibu mara mbili.
- `DROP TABLE` bado inafuta mara moja (si sehemu ya transaction).
- Metadata ya VKFS (superblock, bitmap, directory) si journaled: kukatika kati ya sectors zake kunaweza kuacha nafasi isiyotumika; `vkfs_mount` inairekebisha. Hiyo ni kazi ya v0.3.x.
- Kama kuandika disk kunashindwa kwa kosa la vifaa (si nafasi) baada ya log kuhifadhiwa, COMMIT inabaki kwenye log na inatumika kwenye boot inayofuata (onyo linaonyeshwa).
