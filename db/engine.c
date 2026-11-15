/* VukangDB - SQL parser + executor
 *
 * Supported SQL:
 *   CREATE TABLE t (id INT PRIMARY KEY, name TEXT, ...)
 *   DROP TABLE t
 *   INSERT INTO t VALUES (1, 'Asha', ...)  [, (...)]
 *   SELECT * | col,... | COUNT(*) | SUM(c) | AVG(c) | MIN(c) | MAX(c)
 *          FROM t [WHERE c op v [AND ...]] [ORDER BY c [ASC|DESC]] [LIMIT n]
 *   UPDATE t SET c = v [, ...] [WHERE ...]
 *   DELETE FROM t [WHERE ...]
 *   SHOW TABLES | DESCRIBE t | BEGIN | COMMIT | ROLLBACK
 */
#include "db.h"
#include "db_internal.h"
#include "fs.h"
#include "mm.h"
#include "string.h"
#include "console.h"
#include "vga.h"

static token_t tk[MAX_TOKENS];
static int pos;
static char err[96];

#define ERR(...) do { ksnprintf(err, sizeof err, __VA_ARGS__); return -1; } while (0)
static token_t *peek(void) { return &tk[pos]; }
static token_t *next(void) { token_t *t = &tk[pos]; if (t->type != TK_END) pos++; return t; }
static bool is_kw(const char *kw) { return tk[pos].type == TK_IDENT && strcasecmp(tk[pos].text, kw) == 0; }
static bool accept_kw(const char *kw) { if (is_kw(kw)) { pos++; return true; } return false; }
static bool accept_sym(char c) { if (tk[pos].type == TK_SYMBOL && tk[pos].text[0] == c && !tk[pos].text[1]) { pos++; return true; } return false; }
static int expect_kw(const char *kw) { if (!accept_kw(kw)) ERR("expected %s near '%s'", kw, peek()->text); return 0; }
static int expect_sym(char c) { if (!accept_sym(c)) ERR("expected '%c' near '%s'", c, peek()->text); return 0; }
static int ident(char *out) {
    if (peek()->type != TK_IDENT) ERR("expected a name near '%s'", peek()->text);
    strncpy(out, next()->text, DB_NAME_LEN - 1); out[DB_NAME_LEN - 1] = 0; return 0;
}

/* ---------- WHERE clause ---------- */
typedef struct { int col; char op[3]; int num; char str[DB_TEXT_LEN]; } cond_t;
static cond_t conds[4];
static int nconds;

static int parse_where(table_t *t) {
    nconds = 0;
    if (!accept_kw("WHERE")) return 0;
    do {
        if (nconds >= 4) ERR("too many WHERE conditions (max 4)");
        cond_t *c = &conds[nconds];
        char cn[DB_NAME_LEN];
        if (ident(cn)) return -1;
        if ((c->col = col_index(t, cn)) < 0) ERR("no column '%s'", cn);
        token_t *op = next();
        if (op->type != TK_SYMBOL || !strchr("=<>!", op->text[0])) ERR("expected comparison operator");
        strcpy(c->op, op->text);
        token_t *v = next();
        if (col_type(t, c->col) == COL_INT) { if (v->type != TK_NUMBER) ERR("column '%s' needs a number", cn); c->num = v->num; }
        else { if (v->type != TK_STRING) ERR("column '%s' needs 'text'", cn); strncpy(c->str, v->text, DB_TEXT_LEN - 1); }
        nconds++;
    } while (accept_kw("AND"));
    return 0;
}
static bool test(int cmp, const char *op) {
    if (!strcmp(op, "=")) return cmp == 0;
    if (!strcmp(op, "!=")) return cmp != 0;
    if (!strcmp(op, "<")) return cmp < 0;
    if (!strcmp(op, ">")) return cmp > 0;
    if (!strcmp(op, "<=")) return cmp <= 0;
    if (!strcmp(op, ">=")) return cmp >= 0;
    return false;
}
static int cmp_val(table_t *t, uint8_t *row, int col, int num, const char *str) {
    uint8_t *p = row + col_offset(t, col);
    if (col_type(t, col) == COL_INT) { int v = *(int *)p; return v < num ? -1 : v > num; }
    return strcmp((char *)p, str);
}
static bool row_matches(table_t *t, uint8_t *row) {
    if (!row[0]) return false;
    for (int i = 0; i < nconds; i++)
        if (!test(cmp_val(t, row, conds[i].col, conds[i].num, conds[i].str), conds[i].op)) return false;
    return true;
}

/* ---------- auto-commit helper ---------- */
/* In auto-commit mode a failed write discards the cached table so it is
 * reloaded from disk: memory never holds changes the disk does not have. */
static int done_write(table_t *t) {
    t->dirty = true;
    if (!db_in_txn && !tbl_flush(t)) {
        tbl_discard(t);
        ERR("disk write failed (disk full?) - statement not saved, table unchanged");
    }
    return 0;
}

/* ---------- statements ---------- */
static int st_create(void) {
    char name[DB_NAME_LEN];
    if (expect_kw("TABLE") || ident(name)) return -1;
    if (tbl_open(name)) ERR("table '%s' already exists", name);
    if (expect_sym('(')) return -1;
    column_t cols[DB_MAX_COLS]; int n = 0;
    memset(cols, 0, sizeof cols);
    do {
        if (n >= DB_MAX_COLS) ERR("too many columns (max %d)", DB_MAX_COLS);
        if (ident(cols[n].name)) return -1;
        if (accept_kw("INT") || accept_kw("INTEGER")) cols[n].type = COL_INT;
        else if (accept_kw("TEXT") || accept_kw("VARCHAR")) cols[n].type = COL_TEXT;
        else ERR("unknown type for '%s' (use INT or TEXT)", cols[n].name);
        if (accept_kw("PRIMARY")) { if (expect_kw("KEY")) return -1; cols[n].type |= COL_PK; }
        for (int i = 0; i < n; i++) if (!strcasecmp(cols[i].name, cols[n].name)) ERR("duplicate column '%s'", cols[n].name);
        n++;
    } while (accept_sym(','));
    if (expect_sym(')')) return -1;
    table_t *t = tbl_create(name, cols, n);
    if (!t) ERR("cannot create table (cache full?)");
    if (done_write(t)) return -1;
    kprintf("Table '%s' created (%d columns).\n", name, n);
    return 0;
}

static int st_drop(void) {
    char name[DB_NAME_LEN];
    if (expect_kw("TABLE") || ident(name)) return -1;
    if (db_in_txn) ERR("DROP is not allowed inside a transaction");
    if (!tbl_open(name)) ERR("no such table '%s'", name);
    if (!tbl_drop(name)) ERR("drop failed");
    kprintf("Table '%s' dropped.\n", name);
    return 0;
}

static int pk_conflict(table_t *t, uint8_t *self, int col, uint8_t *val) {
    uint32_t off = col_offset(t, col), sz = col_type(t, col) == COL_INT ? 4 : DB_TEXT_LEN;
    for (uint32_t s = 0; s < t->hdr.nslots; s++) {
        uint8_t *r = tbl_row(t, s);
        if (r[0] && r != self && memcmp(r + off, val, sz) == 0) return 1;
    }
    return 0;
}

/* INSERT is all-or-nothing: every slot added by this statement is recorded
 * in added[]; if any tuple fails (syntax, type, duplicate key, memory) all of
 * them are removed again before the error is reported. */
#define MAX_STMT_ROWS 64

static void undo_rows(table_t *t, const uint32_t *added, int n) {
    for (int k = 0; k < n; k++) {
        uint8_t *r = tbl_row(t, added[k]);
        if (r[0]) { r[0] = 0; t->hdr.nlive--; }
    }
}

static int insert_tuples(table_t *t, uint32_t *added, int *count) {
    do {
        if (*count >= MAX_STMT_ROWS) ERR("too many rows in one INSERT (max %d)", MAX_STMT_ROWS);
        if (expect_sym('(')) return -1;
        uint8_t tmp[1 + DB_MAX_COLS * DB_TEXT_LEN];
        memset(tmp, 0, sizeof tmp); tmp[0] = 1;
        for (uint32_t c = 0; c < t->hdr.ncols; c++) {
            if (c && expect_sym(',')) return -1;
            token_t *v = next();
            uint8_t *p = tmp + col_offset(t, c);
            if (col_type(t, c) == COL_INT) { if (v->type != TK_NUMBER) ERR("column '%s' expects INT", t->hdr.cols[c].name); *(int *)p = v->num; }
            else { if (v->type != TK_STRING) ERR("column '%s' expects TEXT in quotes", t->hdr.cols[c].name); strncpy((char *)p, v->text, DB_TEXT_LEN - 1); }
            if ((t->hdr.cols[c].type & COL_PK) && pk_conflict(t, 0, c, p)) ERR("duplicate PRIMARY KEY for '%s'", t->hdr.cols[c].name);
        }
        if (expect_sym(')')) return -1;
        uint8_t *row = tbl_new_row(t);
        if (!row) ERR("out of memory");
        memcpy(row, tmp, t->hdr.row_size);
        added[(*count)++] = (uint32_t)(row - t->rows) / t->hdr.row_size;
    } while (accept_sym(','));
    return 0;
}

static int st_insert(void) {
    char name[DB_NAME_LEN];
    if (expect_kw("INTO") || ident(name)) return -1;
    table_t *t = tbl_open(name);
    if (!t) ERR("no such table '%s'", name);
    if (expect_kw("VALUES")) return -1;
    uint32_t added[MAX_STMT_ROWS];
    int count = 0;
    if (insert_tuples(t, added, &count)) {
        undo_rows(t, added, count);
        if (count) kprintf("(%d row(s) of this statement undone)\n", count);
        return -1;
    }
    if (done_write(t)) return -1;
    kprintf("%d row(s) inserted.\n", count);
    return 0;
}

static void print_line(int *w, int n) {
    console_putc('+');
    for (int i = 0; i < n; i++) { for (int k = 0; k < w[i] + 2; k++) console_putc('-'); console_putc('+'); }
    console_putc('\n');
}
static void cell_text(table_t *t, uint8_t *row, int c, char *out) {
    uint8_t *p = row + col_offset(t, c);
    if (col_type(t, c) == COL_INT) ksnprintf(out, 40, "%d", *(int *)p);
    else ksnprintf(out, 40, "%s", (char *)p);
}

static int st_select(void) {
    int sel[DB_MAX_COLS], nsel = 0, star = 0, agg = 0, aggcol = -1;
    char aggname[8] = {0}, colnames[DB_MAX_COLS][DB_NAME_LEN];
    if (accept_sym('*')) star = 1;
    else if (is_kw("COUNT") || is_kw("SUM") || is_kw("AVG") || is_kw("MIN") || is_kw("MAX")) {
        strncpy(aggname, next()->text, 7); for (int i = 0; aggname[i]; i++) aggname[i] = toupper(aggname[i]);
        agg = 1;
        if (expect_sym('(')) return -1;
        if (accept_sym('*')) { if (strcmp(aggname, "COUNT")) ERR("%s(*) is not valid", aggname); }
        else if (ident(colnames[0])) return -1; else aggcol = 0;
        if (expect_sym(')')) return -1;
    } else {
        do { if (nsel >= DB_MAX_COLS || ident(colnames[nsel])) return -1; nsel++; } while (accept_sym(','));
    }
    char name[DB_NAME_LEN];
    if (expect_kw("FROM") || ident(name)) return -1;
    table_t *t = tbl_open(name);
    if (!t) ERR("no such table '%s'", name);
    if (star) { nsel = t->hdr.ncols; for (int i = 0; i < nsel; i++) sel[i] = i; }
    else if (!agg) for (int i = 0; i < nsel; i++) if ((sel[i] = col_index(t, colnames[i])) < 0) ERR("no column '%s'", colnames[i]);
    if (agg && aggcol == 0) {
        if ((aggcol = col_index(t, colnames[0])) < 0) ERR("no column '%s'", colnames[0]);
        if (col_type(t, aggcol) != COL_INT && strcmp(aggname, "COUNT")) ERR("%s needs an INT column", aggname);
    }
    if (parse_where(t)) return -1;
    int order = -1, desc = 0, limit = -1;
    if (accept_kw("ORDER")) {
        char oc[DB_NAME_LEN];
        if (expect_kw("BY") || ident(oc)) return -1;
        if ((order = col_index(t, oc)) < 0) ERR("no column '%s'", oc);
        if (accept_kw("DESC")) desc = 1; else accept_kw("ASC");
    }
    if (accept_kw("LIMIT")) { token_t *v = next(); if (v->type != TK_NUMBER) ERR("LIMIT needs a number"); limit = v->num; }

    /* collect matching rows */
    uint32_t *idx = kmalloc((t->hdr.nslots + 1) * sizeof(uint32_t));
    if (!idx) ERR("out of memory");
    uint32_t m = 0;
    for (uint32_t s = 0; s < t->hdr.nslots; s++) if (row_matches(t, tbl_row(t, s))) idx[m++] = s;

    if (agg) {
        int count = 0, sum = 0, mn = 0, mx = 0;
        for (uint32_t i = 0; i < m; i++) {
            count++;
            if (aggcol >= 0 && col_type(t, aggcol) == COL_INT) {
                int v = *(int *)(tbl_row(t, idx[i]) + col_offset(t, aggcol));
                sum += v; if (i == 0 || v < mn) mn = v; if (i == 0 || v > mx) mx = v;
            }
        }
        int r = !strcmp(aggname, "COUNT") ? count : !strcmp(aggname, "SUM") ? sum :
                !strcmp(aggname, "AVG") ? (count ? sum / count : 0) : !strcmp(aggname, "MIN") ? mn : mx;
        kprintf("%s = %d\n", aggname, r);
        kfree(idx);
        return 0;
    }
    if (order >= 0) {                                   /* insertion sort */
        for (uint32_t i = 1; i < m; i++) {
            uint32_t k = idx[i]; int j = i - 1;
            while (j >= 0) {
                uint8_t *a = tbl_row(t, idx[j]), *b = tbl_row(t, k);
                uint8_t *pb = b + col_offset(t, order);
                int c = cmp_val(t, a, order, *(int *)pb, (char *)pb);
                if (desc ? c >= 0 : c <= 0) break;
                idx[j + 1] = idx[j]; j--;
            }
            idx[j + 1] = k;
        }
    }
    if (limit >= 0 && (uint32_t)limit < m) m = limit;

    int w[DB_MAX_COLS]; char cell[40];
    for (int i = 0; i < nsel; i++) w[i] = strlen(t->hdr.cols[sel[i]].name);
    for (uint32_t r = 0; r < m; r++)
        for (int i = 0; i < nsel; i++) { cell_text(t, tbl_row(t, idx[r]), sel[i], cell); int l = strlen(cell); if (l > w[i]) w[i] = l; }
    print_line(w, nsel);
    console_putc('|');
    uint8_t old = vga_get_color(); vga_set_color(VGA_YELLOW, old >> 4);
    for (int i = 0; i < nsel; i++) kprintf(" %-*s |", w[i], t->hdr.cols[sel[i]].name);
    vga_set_color(old & 0x0F, old >> 4);
    console_putc('\n');
    print_line(w, nsel);
    for (uint32_t r = 0; r < m; r++) {
        console_putc('|');
        for (int i = 0; i < nsel; i++) {
            cell_text(t, tbl_row(t, idx[r]), sel[i], cell);
            if (col_type(t, sel[i]) == COL_INT) kprintf(" %*s |", w[i], cell); else kprintf(" %-*s |", w[i], cell);
        }
        console_putc('\n');
    }
    print_line(w, nsel);
    kprintf("%u row(s)%s\n", m, db_in_txn ? "  [in transaction]" : "");
    kfree(idx);
    return 0;
}

static int st_update(void) {
    char name[DB_NAME_LEN];
    if (ident(name)) return -1;
    table_t *t = tbl_open(name);
    if (!t) ERR("no such table '%s'", name);
    if (expect_kw("SET")) return -1;
    int scol[DB_MAX_COLS], ns = 0; token_t sval[DB_MAX_COLS];
    do {
        char cn[DB_NAME_LEN];
        if (ns >= DB_MAX_COLS || ident(cn)) return -1;
        if ((scol[ns] = col_index(t, cn)) < 0) ERR("no column '%s'", cn);
        if (expect_sym('=')) return -1;
        sval[ns] = *next();
        if (col_type(t, scol[ns]) == COL_INT ? sval[ns].type != TK_NUMBER : sval[ns].type != TK_STRING) ERR("wrong value type for '%s'", cn);
        ns++;
    } while (accept_sym(','));
    if (parse_where(t)) return -1;

    /* Pass 1 - check: nothing is modified until every rule holds. */
    uint32_t matches = 0; uint8_t *only = 0;
    for (uint32_t s = 0; s < t->hdr.nslots; s++)
        if (row_matches(t, tbl_row(t, s))) { matches++; only = tbl_row(t, s); }
    for (int i = 0; i < ns && matches; i++) {
        if (!(t->hdr.cols[scol[i]].type & COL_PK)) continue;
        if (matches > 1)
            ERR("UPDATE would give %u rows the same PRIMARY KEY '%s'", matches, t->hdr.cols[scol[i]].name);
        uint8_t val[DB_TEXT_LEN];
        memset(val, 0, sizeof val);
        if (col_type(t, scol[i]) == COL_INT) *(int *)val = sval[i].num;
        else strncpy((char *)val, sval[i].text, DB_TEXT_LEN - 1);
        if (pk_conflict(t, only, scol[i], val)) ERR("duplicate PRIMARY KEY for '%s'", t->hdr.cols[scol[i]].name);
    }

    /* Pass 2 - apply. */
    int n = 0;
    for (uint32_t s = 0; s < t->hdr.nslots; s++) {
        uint8_t *r = tbl_row(t, s);
        if (!row_matches(t, r)) continue;
        for (int i = 0; i < ns; i++) {
            uint8_t *p = r + col_offset(t, scol[i]);
            if (col_type(t, scol[i]) == COL_INT) *(int *)p = sval[i].num;
            else { memset(p, 0, DB_TEXT_LEN); strncpy((char *)p, sval[i].text, DB_TEXT_LEN - 1); }
        }
        n++;
    }
    if (n && done_write(t)) return -1;
    kprintf("%d row(s) updated.\n", n);
    return 0;
}

static int st_delete(void) {
    char name[DB_NAME_LEN];
    if (expect_kw("FROM") || ident(name)) return -1;
    table_t *t = tbl_open(name);
    if (!t) ERR("no such table '%s'", name);
    if (parse_where(t)) return -1;
    int n = 0;
    for (uint32_t s = 0; s < t->hdr.nslots; s++) {
        uint8_t *r = tbl_row(t, s);
        if (row_matches(t, r)) { r[0] = 0; t->hdr.nlive--; n++; }
    }
    if (n && done_write(t)) return -1;
    kprintf("%d row(s) deleted.\n", n);
    return 0;
}

static int st_show(void) {
    if (expect_kw("TABLES")) return -1;
    vkfs_entry_t *e = kmalloc(sizeof(vkfs_entry_t) * VKFS_MAX_FILES);
    if (!e) ERR("out of memory");
    int n = vkfs_list(e, VKFS_MAX_FILES), c = 0;
    kprintf("Tables:\n");
    for (int i = 0; i < n; i++)
        if (str_endswith(e[i].name, ".tbl")) {
            char nm[DB_NAME_LEN]; strncpy(nm, e[i].name, DB_NAME_LEN - 1); nm[strlen(e[i].name) - 4] = 0;
            table_t *t = tbl_open(nm);
            kprintf("  %-20s %u rows\n", nm, t ? t->hdr.nlive : 0); c++;
        }
    if (!c) kprintf("  (none) - try: CREATE TABLE students (id INT PRIMARY KEY, name TEXT)\n");
    kfree(e);
    return 0;
}

static int st_describe(void) {
    char name[DB_NAME_LEN];
    if (ident(name)) return -1;
    table_t *t = tbl_open(name);
    if (!t) ERR("no such table '%s'", name);
    kprintf("Table %s  (row size %u bytes, %u rows)\n", t->name, t->hdr.row_size, t->hdr.nlive);
    for (uint32_t i = 0; i < t->hdr.ncols; i++)
        kprintf("  %-20s %-5s %s\n", t->hdr.cols[i].name, col_type(t, i) == COL_INT ? "INT" : "TEXT",
                (t->hdr.cols[i].type & COL_PK) ? "PRIMARY KEY" : "");
    return 0;
}

static int exec_one(void) {
    if (accept_kw("CREATE")) return st_create();
    if (accept_kw("DROP")) return st_drop();
    if (accept_kw("INSERT")) return st_insert();
    if (accept_kw("SELECT")) return st_select();
    if (accept_kw("UPDATE")) return st_update();
    if (accept_kw("DELETE")) return st_delete();
    if (accept_kw("SHOW")) return st_show();
    if (accept_kw("DESCRIBE") || accept_kw("DESC")) return st_describe();
    if (accept_kw("BEGIN")) { if (db_in_txn) ERR("already in a transaction"); db_in_txn = true; kprintf("Transaction started.\n"); return 0; }
    if (accept_kw("COMMIT")) { if (!db_in_txn) ERR("no active transaction"); db_in_txn = false; if (!tbl_commit()) ERR("commit failed"); kprintf("Committed to disk.\n"); return 0; }
    if (accept_kw("ROLLBACK")) { if (!db_in_txn) ERR("no active transaction"); db_in_txn = false; tbl_rollback(); kprintf("Rolled back. Changes discarded.\n"); return 0; }
    ERR("unknown statement '%s'", peek()->text);
}

void db_init(void) {
    db_in_txn = false;
    int n = wal_recover();                        /* finish (or discard) a COMMIT that was cut short */
    if (n > 0) kprintf("[vukangdb] recovery: replayed %d table(s) from an interrupted COMMIT\n", n);
}

int db_exec(const char *sql) {
    if (!vkfs_mounted()) { kprintf("VukangDB: no filesystem mounted (run 'format')\n"); return -1; }
    err[0] = 0;
    int n = lex(sql, tk, MAX_TOKENS, err);
    if (n < 0) goto fail;
    pos = 0;
    while (peek()->type != TK_END) {
        if (accept_sym(';')) continue;
        if (exec_one() < 0) goto fail;
        if (peek()->type != TK_END && !accept_sym(';')) { ksnprintf(err, sizeof err, "unexpected '%s'", peek()->text); goto fail; }
    }
    return 0;
fail: {
        uint8_t old = vga_get_color(); vga_set_color(VGA_LRED, old >> 4);
        kprintf("SQL error: %s\n", err);
        vga_set_color(old & 0x0F, old >> 4);
        return -1;
    }
}

void db_repl(void) {
    char line[256];
    kprintf("VukangDB 0.1 - SQL engine inside VUKANG OS. Type HELP or EXIT.\n");
    for (;;) {
        uint8_t old = vga_get_color(); vga_set_color(VGA_LMAGENTA, old >> 4);
        kprintf(db_in_txn ? "vukangdb*> " : "vukangdb> ");
        vga_set_color(old & 0x0F, old >> 4);
        console_readline(line, sizeof line);
        if (!line[0]) continue;
        if (!strcasecmp(line, "exit") || !strcasecmp(line, "quit") || !strcasecmp(line, ".exit")) break;
        if (!strcasecmp(line, "help")) {
            kprintf("  CREATE TABLE t (id INT PRIMARY KEY, name TEXT)\n"
                    "  INSERT INTO t VALUES (1, 'Asha'), (2, 'Juma')\n"
                    "  SELECT * FROM t WHERE id > 1 ORDER BY name DESC LIMIT 10\n"
                    "  SELECT COUNT(*) FROM t   |  SUM/AVG/MIN/MAX(col)\n"
                    "  UPDATE t SET name = 'Neema' WHERE id = 2\n"
                    "  DELETE FROM t WHERE id = 1     DROP TABLE t\n"
                    "  SHOW TABLES   DESCRIBE t   BEGIN / COMMIT / ROLLBACK\n");
            continue;
        }
        db_exec(line);
    }
}
