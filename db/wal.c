/* VukangDB - Write-Ahead Log (WAL)
 *
 * PROBLEM: VKFS overwrites a table file sector by sector. If the power fails
 * half-way, the table is half old, half new. A COMMIT of two tables can also
 * end with one table new and the other old.
 *
 * SOLUTION: before touching any table, write the COMPLETE new images of all
 * changed tables into one log file (".wal") protected by a CRC32. Only then
 * update the tables, and finally delete the log.
 *
 *   1. build log   : header + [name, length, image] for every changed table
 *   2. write ".wal": VKFS writes the data first, the directory entry last, and
 *                    the CRC covers everything -> a log is valid or it is not
 *   3. apply       : vkfs_write() every table image          (can be redone!)
 *   4. checkpoint  : delete ".wal"
 *
 * RECOVERY (at boot, wal_recover):
 *   - no .wal                  -> nothing to do
 *   - .wal with a valid CRC    -> the COMMIT happened: redo step 3 and 4
 *   - .wal with a bad CRC      -> the COMMIT never completed: throw it away
 * Redo is safe to repeat because it writes whole images (idempotent).
 *
 * The COMMIT POINT is step 2: once a valid .wal exists, the transaction is
 * committed, even if the power fails right afterwards. */
#include "db_internal.h"
#include "fs.h"
#include "mm.h"
#include "string.h"
#include "console.h"

#define WAL_MAGIC   0x4C415756u        /* "VWAL" */
#define WAL_VERSION 1

typedef struct { uint32_t magic, version, seq, nitems, total, crc; } wal_hdr_t;
typedef struct { char name[40]; uint32_t len; } wal_rec_t;      /* followed by len bytes unless WAL_DELETE */

int wal_crash_point;                   /* debug: simulate a power cut (see walcrash) */
static uint32_t seq;
static bool pending;                   /* a logged commit that is not applied yet */

static uint32_t crc32(const uint8_t *p, uint32_t n) {
    uint32_t c = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
    }
    return ~c;
}
static uint32_t log_crc(uint8_t *buf, uint32_t total) {          /* CRC with the crc field treated as 0 */
    wal_hdr_t *h = (wal_hdr_t *)buf;
    uint32_t saved = h->crc; h->crc = 0;
    uint32_t c = crc32(buf, total);
    h->crc = saved;
    return c;
}

/* Simulated power cut: stop dead, write nothing more. */
static void crash_if(int point) {
    if (wal_crash_point != point) return;
    __asm__ volatile("cli");
    kprintf("\n*** WAL CRASH POINT %d: power cut simulated ***\n", point);
    for (;;) __asm__ volatile("hlt");
}

static uint8_t *build_log(const wal_item_t *items, int n, uint32_t *total) {
    uint32_t sz = sizeof(wal_hdr_t);
    for (int i = 0; i < n; i++) sz += sizeof(wal_rec_t) + (items[i].len == WAL_DELETE ? 0 : items[i].len);
    if (sz > WAL_MAX) { kprintf("[vukangdb] transaction too large for the log (%u > %u bytes)\n", sz, WAL_MAX); return 0; }
    uint8_t *buf = kmalloc(sz);
    if (!buf) return 0;
    wal_hdr_t *h = (wal_hdr_t *)buf;
    h->magic = WAL_MAGIC; h->version = WAL_VERSION; h->seq = ++seq; h->nitems = n; h->total = sz; h->crc = 0;
    uint32_t off = sizeof *h;
    for (int i = 0; i < n; i++) {
        wal_rec_t r; memset(&r, 0, sizeof r);
        strcpy(r.name, items[i].name); r.len = items[i].len;
        memcpy(buf + off, &r, sizeof r); off += sizeof r;
        if (r.len != WAL_DELETE) { memcpy(buf + off, items[i].data, r.len); off += r.len; }
    }
    h->crc = crc32(buf, sz);                        /* crc field is 0 while computing */
    *total = sz;
    return buf;
}

static bool name_ok(const char *n) {            /* non-empty and NUL-terminated inside the field */
    for (uint32_t i = 0; i < 40; i++) if (!n[i]) return i > 0;
    return false;
}

/* Is this byte string a complete, well-formed log? (never trust the disk) */
static bool log_valid(uint8_t *buf, uint32_t sz) {
    if (sz < sizeof(wal_hdr_t)) return false;
    wal_hdr_t *h = (wal_hdr_t *)buf;
    if (h->magic != WAL_MAGIC || h->version != WAL_VERSION || h->total != sz) return false;
    if (log_crc(buf, sz) != h->crc) return false;
    uint32_t off = sizeof *h;
    for (uint32_t i = 0; i < h->nitems; i++) {
        if (sz - off < sizeof(wal_rec_t)) return false;
        wal_rec_t *r = (wal_rec_t *)(buf + off); off += sizeof *r;
        if (!name_ok(r->name)) return false;
        if (r->len != WAL_DELETE) { if (r->len > sz - off) return false; off += r->len; }
    }
    return off == sz;
}

/* Step 3: write every image to its table file. Used by commit AND recovery. */
static bool apply_log(uint8_t *buf, bool tear_first) {
    wal_hdr_t *h = (wal_hdr_t *)buf;
    uint32_t off = sizeof *h; bool ok = true;
    for (uint32_t i = 0; i < h->nitems; i++) {
        wal_rec_t *r = (wal_rec_t *)(buf + off); off += sizeof *r;
        if (r->len == WAL_DELETE) { if (vkfs_exists(r->name) && !vkfs_delete(r->name)) ok = false; continue; }
        if (tear_first) {                           /* crash test: half of the first image reaches the disk */
            vkfs_write(r->name, buf + off, r->len / 2);
            crash_if(3);
            tear_first = false;
        }
        if (!vkfs_write(r->name, buf + off, r->len)) ok = false;
        off += r->len;
    }
    return ok;
}

wal_result_t wal_commit(const wal_item_t *items, int n) {
    if (n <= 0) return WAL_OK;
    if (pending && wal_recover() < 0) return WAL_FAILED;      /* finish the previous commit first */
    crash_if(1);                                              /* nothing logged yet */
    uint32_t total; uint8_t *buf = build_log(items, n, &total);
    if (!buf) return WAL_FAILED;

    /* Dry run first: the log AND every new table image must fit. If not, refuse
     * BEFORE touching the disk, so a committed log can never fail to apply. */
    bool fits = vkfs_sim_begin();
    if (fits) {
        fits = vkfs_write(WAL_FILE, buf, total);
        for (int i = 0; fits && i < n; i++)
            fits = items[i].len == WAL_DELETE ? true : vkfs_write(items[i].name, items[i].data, items[i].len);
        vkfs_sim_end();
    }
    if (!fits) {
        kprintf("[vukangdb] not enough free disk space for this COMMIT (the log needs room for the new table images)\n");
        kfree(buf); return WAL_FAILED;
    }

    if (wal_crash_point == 5) ((wal_hdr_t *)buf)->crc ^= 1;   /* crash test: a torn / corrupt log */
    if (!vkfs_write(WAL_FILE, buf, total)) {                  /* cannot log (disk full): nothing changed */
        kfree(buf); return WAL_FAILED;
    }
    crash_if(5);
    crash_if(2);                                              /* COMMITTED, nothing applied */
    bool ok = apply_log(buf, wal_crash_point == 3);
    kfree(buf);
    if (!ok) {
        pending = true;
        kprintf("[vukangdb] warning: COMMIT is safe in the log but not applied yet (disk full?); it will be applied at the next boot\n");
        return WAL_PENDING;
    }
    crash_if(4);                                              /* all applied, log not removed yet */
    vkfs_delete(WAL_FILE);                                    /* checkpoint */
    return WAL_OK;
}

int wal_recover(void) {
    int sz = vkfs_size(WAL_FILE);
    if (sz < 0) { pending = false; return 0; }
    if (sz == 0 || sz > WAL_MAX) {
        kprintf("[vukangdb] recovery: invalid log removed\n");
        vkfs_delete(WAL_FILE); pending = false; return 0;
    }
    uint8_t *buf = kmalloc(sz);
    if (!buf) return -1;
    vkfs_read(WAL_FILE, buf, sz);
    if (!log_valid(buf, sz)) {
        kprintf("[vukangdb] recovery: incomplete log ignored (that COMMIT never finished, tables unchanged)\n");
        kfree(buf); vkfs_delete(WAL_FILE); pending = false; return 0;
    }
    int n = (int)((wal_hdr_t *)buf)->nitems;
    seq = ((wal_hdr_t *)buf)->seq;
    bool ok = apply_log(buf, false);
    kfree(buf);
    if (!ok) { kprintf("[vukangdb] recovery: could not replay the log (disk full?) - kept for the next boot\n"); pending = true; return -1; }
    vkfs_delete(WAL_FILE);
    pending = false;
    return n;
}
