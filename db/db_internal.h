/* VukangDB internal definitions */
#ifndef VK_DB_INTERNAL_H
#define VK_DB_INTERNAL_H
#include <stdint.h>
#include <stdbool.h>

/* ---------- lexer ---------- */
typedef enum { TK_END, TK_IDENT, TK_NUMBER, TK_STRING, TK_SYMBOL } tok_type_t;
typedef struct { tok_type_t type; char text[64]; int num; } token_t;
#define MAX_TOKENS 160
int lex(const char *sql, token_t *out, int max, char *err);

/* ---------- storage ---------- */
#define DB_MAGIC     0x42444B56   /* "VKDB" */
#define DB_MAX_COLS  8
#define DB_NAME_LEN  24
#define DB_TEXT_LEN  32
#define DB_MAX_TABLES 16
enum { COL_INT = 1, COL_TEXT = 2 };
#define COL_PK 0x80

typedef struct {
    char    name[DB_NAME_LEN];
    uint8_t type;      /* COL_INT / COL_TEXT, | COL_PK for primary key */
    uint8_t pad[3];
} __attribute__((packed)) column_t;

typedef struct {
    uint32_t magic, version, ncols, nslots, nlive, row_size, reserved[2];
    column_t cols[DB_MAX_COLS];
    uint8_t  pad[512 - 32 - DB_MAX_COLS * 28];
} __attribute__((packed)) table_hdr_t;

typedef struct {
    bool        used, dirty, on_disk;
    char        name[DB_NAME_LEN];
    table_hdr_t hdr;
    uint8_t    *rows;      /* nslots * row_size bytes; byte 0 of each row = valid flag */
    uint32_t    capacity;  /* slots allocated in memory */
} table_t;

table_t *tbl_open(const char *name);                /* cached load */
table_t *tbl_create(const char *name, column_t *cols, int ncols);
bool     tbl_flush(table_t *t);
bool     tbl_drop(const char *name);
void     tbl_rollback(void);
bool     tbl_commit(void);
uint8_t *tbl_row(table_t *t, uint32_t slot);
uint8_t *tbl_new_row(table_t *t);
int      col_index(table_t *t, const char *name);
uint32_t col_offset(table_t *t, int c);
int      col_type(table_t *t, int c);
extern bool db_in_txn;
#endif
