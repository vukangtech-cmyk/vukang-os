#ifndef VK_DB_H
#define VK_DB_H
/* VukangDB - SQL database engine built into VUKANG OS */
void db_init(void);
int  db_exec(const char *sql);   /* 0 = ok, <0 = error */
void db_repl(void);
#endif
