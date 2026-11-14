/* VukangDB - SQL tokenizer */
#include "db_internal.h"
#include "string.h"
#include "console.h"

int lex(const char *s, token_t *out, int max, char *err) {
    int n = 0;
    while (*s) {
        if (isspace(*s)) { s++; continue; }
        if (n >= max - 1) { strcpy(err, "statement too long"); return -1; }
        token_t *t = &out[n];
        memset(t, 0, sizeof *t);
        if (isalpha(*s)) {
            int i = 0;
            while ((isalpha(*s) || isdigit(*s)) && i < 63) t->text[i++] = *s++;
            t->type = TK_IDENT;
        } else if (isdigit(*s) || (*s == '-' && isdigit(s[1]))) {
            int i = 0;
            if (*s == '-') t->text[i++] = *s++;
            while (isdigit(*s) && i < 63) t->text[i++] = *s++;
            t->type = TK_NUMBER; t->num = atoi(t->text);
        } else if (*s == '\'' || *s == '"') {
            char q = *s++; int i = 0;
            while (*s && *s != q) { if (i < 63) t->text[i++] = *s; s++; }
            if (*s != q) { strcpy(err, "unterminated string"); return -1; }
            s++; t->type = TK_STRING;
        } else if ((s[0] == '!' || s[0] == '<' || s[0] == '>') && s[1] == '=') {
            t->text[0] = s[0]; t->text[1] = '='; s += 2; t->type = TK_SYMBOL;
        } else if (s[0] == '<' && s[1] == '>') {
            strcpy(t->text, "!="); s += 2; t->type = TK_SYMBOL;
        } else if (strchr("(),*=<>;", *s)) {
            t->text[0] = *s++; t->type = TK_SYMBOL;
        } else {
            ksnprintf(err, 64, "unexpected character '%c'", *s); return -1;
        }
        n++;
    }
    out[n].type = TK_END; out[n].text[0] = 0;
    return n;
}
