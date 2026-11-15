/* VUKANG OS - fs/vkfs.c : VKFS, the VUKANG File System
 *
 * On-disk layout (512-byte sectors):
 *   LBA 0            reserved (boot sector)
 *   LBA 1            superblock
 *   LBA 2..          free-space bitmap (1 bit per sector)
 *   next 8 sectors   directory: 64 entries x 64 bytes
 *   rest             file data, each file stored as one contiguous extent
 */
#include "fs.h"
#include "drivers.h"
#include "string.h"
#include "console.h"
#include "io.h"

#define VKFS_MAGIC   0x53464B56   /* "VKFS" */
#define VKFS_VERSION 1
#define MAX_SECTORS  131072       /* 64 MiB supported */
#define DIR_SECTORS  8

typedef struct {
    uint32_t magic, version, total, bitmap_start, bitmap_sectors, dir_start, data_start, files;
    char     label[32];
    uint8_t  pad[512 - 64];
} __attribute__((packed)) superblock_t;

static superblock_t sb;
static uint8_t bitmap[MAX_SECTORS / 8];
static vkfs_entry_t dir[VKFS_MAX_FILES];
static bool mounted;
static blockdev_t *dev;

static int  bget(uint32_t s) { return bitmap[s >> 3] & (1 << (s & 7)); }
static void bset(uint32_t s, int v) { if (v) bitmap[s >> 3] |= 1 << (s & 7); else bitmap[s >> 3] &= ~(1 << (s & 7)); }

/* ---- dry run ("would these writes fit?") ------------------------------------
 * The WAL needs room for the log AND the new table images. Instead of guessing,
 * it replays the real allocation code on a scratch copy of the metadata with
 * disk writes switched off (vkfs_sim_begin ... vkfs_sim_end). Interrupts stay
 * off meanwhile, so no other task can see the fake state. */
static bool sim_on;
static uint32_t sim_flags;
static superblock_t sim_sb;
static uint8_t sim_bitmap[MAX_SECTORS / 8];
static vkfs_entry_t sim_dir[VKFS_MAX_FILES];
static bool devw(uint32_t lba, uint8_t n, const void *b) { return sim_on ? true : dev->write(lba, n, b); }

bool vkfs_sim_begin(void) {
    if (!mounted || sim_on) return false;
    sim_flags = irq_save();
    memcpy(&sim_sb, &sb, sizeof sb); memcpy(sim_bitmap, bitmap, sizeof bitmap); memcpy(sim_dir, dir, sizeof dir);
    sim_on = true;
    return true;
}
void vkfs_sim_end(void) {
    if (!sim_on) return;
    memcpy(&sb, &sim_sb, sizeof sb); memcpy(bitmap, sim_bitmap, sizeof bitmap); memcpy(dir, sim_dir, sizeof dir);
    sim_on = false;
    irq_restore(sim_flags);
}

static bool flush_meta(void) {
    bool ok = devw(1, 1, &sb);
    for (uint32_t i = 0; i < sb.bitmap_sectors; i++) ok &= devw(sb.bitmap_start + i, 1, bitmap + i * 512);
    for (uint32_t i = 0; i < DIR_SECTORS; i++) ok &= devw(sb.dir_start + i, 1, (uint8_t *)dir + i * 512);
    return ok;
}

bool vkfs_format(void) {
    dev = blockdev_get();
    memset(&sb, 0, sizeof sb);
    sb.magic = VKFS_MAGIC; sb.version = VKFS_VERSION;
    sb.total = dev->sectors > MAX_SECTORS ? MAX_SECTORS : dev->sectors;
    sb.bitmap_start = 2;
    sb.bitmap_sectors = (sb.total + 4095) / 4096;
    sb.dir_start = sb.bitmap_start + sb.bitmap_sectors;
    sb.data_start = sb.dir_start + DIR_SECTORS;
    strcpy(sb.label, "VUKANG");
    memset(bitmap, 0, sizeof bitmap);
    for (uint32_t s = 0; s < sb.data_start; s++) bset(s, 1);
    memset(dir, 0, sizeof dir);
    mounted = flush_meta();
    return mounted;
}

/* Never trust the disk: a corrupted or malicious superblock could otherwise
 * make us read past bitmap[] / dir[] (buffer overflow) or write anywhere. */
static bool sb_valid(void) {
    if (sb.magic != VKFS_MAGIC || sb.version != VKFS_VERSION) return false;
    if (sb.total < 64 || sb.total > MAX_SECTORS || sb.total > dev->sectors) return false;
    if (sb.bitmap_start != 2 || sb.bitmap_sectors != (sb.total + 4095) / 4096) return false;
    if (sb.dir_start != sb.bitmap_start + sb.bitmap_sectors) return false;
    if (sb.data_start != sb.dir_start + DIR_SECTORS || sb.data_start >= sb.total) return false;
    return true;
}
static uint32_t repaired;
bool vkfs_mount(void) {
    dev = blockdev_get();
    mounted = false;
    if (!dev->read(1, 1, &sb) || !sb_valid()) return false;
    for (uint32_t i = 0; i < sb.bitmap_sectors; i++) if (!dev->read(sb.bitmap_start + i, 1, bitmap + i * 512)) return false;
    for (uint32_t i = 0; i < DIR_SECTORS; i++) if (!dev->read(sb.dir_start + i, 1, (uint8_t *)dir + i * 512)) return false;
    for (uint32_t s = 0; s < sb.data_start; s++) bset(s, 1);          /* metadata always reserved */
    repaired = 0; sb.files = 0;
    for (int i = 0; i < VKFS_MAX_FILES; i++) {                         /* validate every entry */
        vkfs_entry_t *e = &dir[i];
        if (!(e->flags & 1)) continue;
        e->name[VKFS_NAME_LEN - 1] = 0;
        bool ok = e->name[0] && e->alloc > 0 && e->start >= sb.data_start && e->start < sb.total &&
                  e->alloc <= sb.total - e->start && e->size <= e->alloc * 512;
        if (!ok) { memset(e, 0, sizeof *e); repaired++; continue; }
        for (uint32_t k = 0; k < e->alloc; k++) bset(e->start + k, 1);
        sb.files++;
    }
    mounted = true;
    if (repaired) { kprintf("[vkfs] removed %u corrupted directory entries\n", repaired); flush_meta(); }
    return true;
}
bool vkfs_mounted(void) { return mounted; }

static int find(const char *name) {
    for (int i = 0; i < VKFS_MAX_FILES; i++)
        if ((dir[i].flags & 1) && strcmp(dir[i].name, name) == 0) return i;
    return -1;
}
static uint32_t alloc_extent(uint32_t n) {        /* first-fit contiguous run */
    uint32_t run = 0;
    for (uint32_t s = sb.data_start; s < sb.total; s++) {
        run = bget(s) ? 0 : run + 1;
        if (run == n) { uint32_t st = s - n + 1; for (uint32_t k = st; k <= s; k++) bset(k, 1); return st; }
    }
    return 0;
}
static void free_extent(uint32_t st, uint32_t n) { for (uint32_t k = 0; k < n; k++) bset(st + k, 0); }

static uint32_t now_packed(void) {
    rtc_time_t t; rtc_read(&t);
    return ((uint32_t)(t.year - 2000) << 26) | (t.month << 22) | (t.day << 17) | (t.hour << 12) | (t.minute << 6) | t.second;
}

/* Grow an extent in place: claim sectors [st+cur, st+want) if all are free. */
static bool extend_extent(uint32_t st, uint32_t cur, uint32_t want) {
    if (want <= cur || st + want > sb.total || st + want < st) return false;
    for (uint32_t k = st + cur; k < st + want; k++) if (bget(k)) return false;
    for (uint32_t k = st + cur; k < st + want; k++) bset(k, 1);
    return true;
}

/* Write a whole file. Failure-safe ordering:
 *   1. reserve space (grow in place, or a NEW extent) while the old one is
 *      still allocated - nothing is released yet;
 *   2. no space -> return false; the existing file is left untouched;
 *   3. write the data; on an I/O error undo the reservation and keep the
 *      old start/alloc;
 *   4. only after the data is on disk: release the old extent, update the
 *      directory entry and flush the metadata. */
bool vkfs_write(const char *name, const void *data, uint32_t len) {
    if (!mounted || !name || !name[0] || strlen(name) >= VKFS_NAME_LEN) return false;
    uint32_t f = irq_save();
    int i = find(name);
    bool is_new = i < 0;
    if (is_new) {
        for (int k = 0; k < VKFS_MAX_FILES; k++) if (!(dir[k].flags & 1)) { i = k; break; }
        if (i < 0) { irq_restore(f); return false; }
    }
    uint32_t old_start = is_new ? 0 : dir[i].start;
    uint32_t old_alloc = is_new ? 0 : dir[i].alloc;
    uint32_t need = (len + 511) / 512; if (!need) need = 1;
    uint32_t st = old_start, alloc = old_alloc;
    bool moved = false;

    if (alloc < need) {
        uint32_t want = need + need / 2;                       /* room to grow */
        if (old_alloc && extend_extent(old_start, old_alloc, want)) alloc = want;
        else if (old_alloc && extend_extent(old_start, old_alloc, need)) alloc = need;
        else {
            uint32_t n = want;
            uint32_t ns = alloc_extent(n);
            if (!ns) { n = need; ns = alloc_extent(n); }
            if (!ns) { irq_restore(f); return false; }        /* disk full: old file intact */
            st = ns; alloc = n; moved = true;
        }
    }

    const uint8_t *p = data;
    uint8_t sec[512];
    for (uint32_t s = 0; s < need; s++) {
        uint32_t off = s * 512, chunk = len > off ? (len - off > 512 ? 512 : len - off) : 0;
        memset(sec, 0, 512);
        if (chunk) memcpy(sec, p + off, chunk);
        if (!devw(st + s, 1, sec)) {                    /* undo the reservation */
            if (moved) free_extent(st, alloc);
            else if (alloc > old_alloc) free_extent(old_start + old_alloc, alloc - old_alloc);
            irq_restore(f);
            return false;
        }
    }

    if (moved && old_alloc) free_extent(old_start, old_alloc);   /* data is safe now */
    if (is_new) { memset(&dir[i], 0, sizeof dir[i]); strcpy(dir[i].name, name); }
    dir[i].start = st; dir[i].alloc = alloc;
    dir[i].size = len; dir[i].flags = 1; dir[i].mtime = now_packed();
    if (is_new) sb.files++;
    bool ok = flush_meta();
    irq_restore(f);
    return ok;
}

int vkfs_read(const char *name, void *buf, uint32_t max) {
    int i = find(name);
    if (!mounted || i < 0) return -1;
    uint32_t len = dir[i].size < max ? dir[i].size : max;
    uint8_t sec[512], *p = buf;
    for (uint32_t off = 0; off < len; off += 512) {
        if (!dev->read(dir[i].start + off / 512, 1, sec)) return -1;
        uint32_t c = len - off > 512 ? 512 : len - off;
        memcpy(p + off, sec, c);
    }
    return (int)len;
}

bool vkfs_delete(const char *name) {
    int i = find(name);
    if (!mounted || i < 0) return false;
    free_extent(dir[i].start, dir[i].alloc);
    memset(&dir[i], 0, sizeof dir[i]);
    sb.files--;
    return flush_meta();
}
bool vkfs_exists(const char *name) { return mounted && find(name) >= 0; }
int  vkfs_size(const char *name) { if (!mounted) return -1; int i = find(name); return i < 0 ? -1 : (int)dir[i].size; }
int  vkfs_list(vkfs_entry_t *out, int max) {
    int n = 0;
    for (int i = 0; i < VKFS_MAX_FILES && n < max; i++) if (dir[i].flags & 1) out[n++] = dir[i];
    return n;
}
void vkfs_info(uint32_t *total, uint32_t *freec, uint32_t *files) {
    uint32_t fr = 0;
    for (uint32_t s = sb.data_start; s < sb.total; s++) if (!bget(s)) fr++;
    *total = sb.total; *freec = fr; *files = sb.files;
}
