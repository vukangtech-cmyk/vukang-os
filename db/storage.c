/* VukangDB - storage engine
 * Each table is one VKFS file "<name>.tbl": a 512-byte header (schema)
 * followed by fixed-size row slots. Tables are cached in memory (a small
 * buffer pool); changes are written back on COMMIT (or immediately in
 * auto-commit mode). ROLLBACK simply throws away dirty cached tables. */
#include "db_internal.h"
#include "fs.h"
#include "mm.h"
#include "string.h"
#include "console.h"

static table_t cache[DB_MAX_TABLES];
bool db_in_txn;

static void fname(char *out, const char *name) { ksnprintf(out, 40, "%s.tbl", name); }
static void lower(char *d, const char *s) { int i = 0; for (; s[i] && i < DB_NAME_LEN - 1; i++) d[i] = tolower(s[i]); d[i] = 0; }

int col_type(table_t *t, int c) { return t->hdr.cols[c].type & 0x7F; }
uint32_t col_offset(table_t *t, int c) {
    uint32_t off = 1;
    for (int i = 0; i < c; i++) off += col_type(t, i) == COL_INT ? 4 : DB_TEXT_LEN;
    return off;
}
int col_index(table_t *t, const char *name) {
    for (uint32_t i = 0; i < t->hdr.ncols; i++) if (strcasecmp(t->hdr.cols[i].name, name) == 0) return i;
    return -1;
}
uint8_t *tbl_row(table_t *t, uint32_t slot) { return t->rows + slot * t->hdr.row_size; }

static bool grow(table_t *t, uint32_t need) {
    if (need <= t->capacity) return true;
    uint32_t cap = t->capacity ? t->capacity * 2 : 16;
    while (cap < need) cap *= 2;
    uint8_t *r = kzalloc(cap * t->hdr.row_size);
    if (!r) return false;
    if (t->rows) { memcpy(r, t->rows, t->hdr.nslots * t->hdr.row_size); kfree(t->rows); }
    t->rows = r; t->capacity = cap;
    return true;
}
uint8_t *tbl_new_row(table_t *t) {
    for (uint32_t s = 0; s < t->hdr.nslots; s++)          /* reuse deleted slot */
        if (!tbl_row(t, s)[0]) { t->hdr.nlive++; return tbl_row(t, s); }
    if (!grow(t, t->hdr.nslots + 1)) return 0;
    uint8_t *r = tbl_row(t, t->hdr.nslots++);
    memset(r, 0, t->hdr.row_size);
    t->hdr.nlive++;
    return r;
}

static table_t *slot_for(const char *name) {
    for (int i = 0; i < DB_MAX_TABLES; i++) if (cache[i].used && strcmp(cache[i].name, name) == 0) return &cache[i];
    for (int i = 0; i < DB_MAX_TABLES; i++) if (!cache[i].used) return &cache[i];
    for (int i = 0; i < DB_MAX_TABLES; i++)                /* evict a clean table */
        if (!cache[i].dirty) { kfree(cache[i].rows); memset(&cache[i], 0, sizeof cache[i]); return &cache[i]; }
    return 0;
}

table_t *tbl_open(const char *raw) {
    char name[DB_NAME_LEN], fn[40]; lower(name, raw);
    table_t *t = slot_for(name);
    if (!t) return 0;
    if (t->used) return t;
    fname(fn, name);
    int sz = vkfs_size(fn);
    if (sz < (int)sizeof(table_hdr_t)) return 0;
    uint8_t *buf = kmalloc(sz);
    if (!buf) return 0;
    vkfs_read(fn, buf, sz);
    memset(t, 0, sizeof *t);
    memcpy(&t->hdr, buf, sizeof t->hdr);
    /* validate the schema read from disk before using any of it */
    bool ok = t->hdr.magic == DB_MAGIC && t->hdr.ncols >= 1 && t->hdr.ncols <= DB_MAX_COLS;
    for (uint32_t c = 0; ok && c < t->hdr.ncols; c++) {
        t->hdr.cols[c].name[DB_NAME_LEN - 1] = 0;
        int ty = t->hdr.cols[c].type & 0x7F;
        ok = t->hdr.cols[c].name[0] && (ty == COL_INT || ty == COL_TEXT);
    }
    if (!ok || t->hdr.row_size != col_offset(t, t->hdr.ncols)) {
        kfree(buf); memset(t, 0, sizeof *t);
        kprintf("[vukangdb] table '%s' is corrupted - refusing to load\n", name);
        return 0;
    }
    strcpy(t->name, name);
    uint32_t fit = (sz - sizeof(table_hdr_t)) / t->hdr.row_size;
    if (t->hdr.nslots > fit) t->hdr.nslots = fit;
    if (!grow(t, t->hdr.nslots ? t->hdr.nslots : 1)) { kfree(buf); return 0; }
    memcpy(t->rows, buf + sizeof(table_hdr_t), t->hdr.nslots * t->hdr.row_size);
    kfree(buf);
    t->hdr.nlive = 0;                                   /* recount + terminate every TEXT field */
    for (uint32_t s = 0; s < t->hdr.nslots; s++) {
        uint8_t *r = tbl_row(t, s);
        if (r[0] != 1) { r[0] = 0; continue; }
        t->hdr.nlive++;
        for (uint32_t c = 0; c < t->hdr.ncols; c++)
            if (col_type(t, c) == COL_TEXT) r[col_offset(t, c) + DB_TEXT_LEN - 1] = 0;
    }
    t->used = true; t->on_disk = true;
    return t;
}

table_t *tbl_create(const char *raw, column_t *cols, int ncols) {
    char name[DB_NAME_LEN]; lower(name, raw);
    table_t *t = slot_for(name);
    if (!t || t->used) return 0;
    memset(t, 0, sizeof *t);
    strcpy(t->name, name);
    t->hdr.magic = DB_MAGIC; t->hdr.version = 1; t->hdr.ncols = ncols;
    memcpy(t->hdr.cols, cols, ncols * sizeof(column_t));
    t->hdr.row_size = col_offset(t, ncols);
    if (!grow(t, 16)) { memset(t, 0, sizeof *t); return 0; }
    t->used = true; t->dirty = true;
    return t;
}

/* Write a table back to its file. The on-disk image is built in a separate
 * buffer (live rows only); the in-memory table is compacted to match it only
 * AFTER vkfs_write succeeded, so a failed write never leaves RAM describing
 * a layout that is not on disk. */
bool tbl_flush(table_t *t) {
    uint32_t live = 0;
    for (uint32_t s = 0; s < t->hdr.nslots; s++) if (tbl_row(t, s)[0]) live++;
    uint32_t sz = sizeof(table_hdr_t) + live * t->hdr.row_size;
    uint8_t *buf = kmalloc(sz);
    if (!buf) return false;
    uint32_t w = 0;
    for (uint32_t s = 0; s < t->hdr.nslots; s++)
        if (tbl_row(t, s)[0]) { memcpy(buf + sizeof(table_hdr_t) + w * t->hdr.row_size, tbl_row(t, s), t->hdr.row_size); w++; }
    table_hdr_t h = t->hdr;
    h.nslots = live; h.nlive = live;
    memcpy(buf, &h, sizeof h);
    char fn[40]; fname(fn, t->name);
    bool ok = vkfs_write(fn, buf, sz);
    if (ok) {                                   /* now RAM may take the disk layout */
        memcpy(t->rows, buf + sizeof(table_hdr_t), live * t->hdr.row_size);
        t->hdr.nslots = live; t->hdr.nlive = live;
        t->dirty = false; t->on_disk = true;
    }
    kfree(buf);
    return ok;
}

/* Throw away the cached copy of a table; the next tbl_open() reloads it from
 * disk, so memory can never disagree with what is actually stored. */
void tbl_discard(table_t *t) {
    if (!t || !t->used) return;
    kfree(t->rows);
    memset(t, 0, sizeof *t);
}

bool tbl_drop(const char *raw) {
    char name[DB_NAME_LEN], fn[40]; lower(name, raw);
    for (int i = 0; i < DB_MAX_TABLES; i++)
        if (cache[i].used && strcmp(cache[i].name, name) == 0) { kfree(cache[i].rows); memset(&cache[i], 0, sizeof cache[i]); }
    fname(fn, name);
    return vkfs_delete(fn);
}

bool tbl_commit(void) {
    bool ok = true;
    for (int i = 0; i < DB_MAX_TABLES; i++)
        if (cache[i].used && cache[i].dirty && !tbl_flush(&cache[i])) { tbl_discard(&cache[i]); ok = false; }
    return ok;
}
void tbl_rollback(void) {
    for (int i = 0; i < DB_MAX_TABLES; i++)
        if (cache[i].used && cache[i].dirty) tbl_discard(&cache[i]);
}
