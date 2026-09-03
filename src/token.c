#include "token.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const TYPE_NAMES[TT_COUNT] = {
    "INT", "IF", "ELSE", "WHILE", "PRINT",
    "ID", "NUM",
    "ASSIGN", "PLUS", "MINUS", "STAR", "SLASH",
    "LT", "GT", "LE", "GE", "EQ", "NE",
    "LPAREN", "RPAREN", "LBRACE", "RBRACE", "SEMI",
    "EOF"
};

/* NULL means "this type has no fixed spelling, the lexeme must be given". */
static const char *const DEFAULT_LEXEME[TT_COUNT] = {
    "int", "if", "else", "while", "print",
    NULL, NULL,                       /* ID, NUM */
    "=", "+", "-", "*", "/",
    "<", ">", "<=", ">=", "==", "!=",
    "(", ")", "{", "}", ";",
    ""                                /* EOF */
};

const char *token_type_name(TokenType t)
{
    return (t >= 0 && t < TT_COUNT) ? TYPE_NAMES[t] : "?";
}

const char *token_default_lexeme(TokenType t)
{
    return (t >= 0 && t < TT_COUNT) ? DEFAULT_LEXEME[t] : NULL;
}

TokenType token_type_from_name(const char *name)
{
    for (int i = 0; i < TT_COUNT; i++) {
        const char *cand = TYPE_NAMES[i];
        size_t j = 0;
        for (; name[j] && cand[j]; j++) {
            if (toupper((unsigned char)name[j]) != cand[j]) break;
        }
        if (name[j] == '\0' && cand[j] == '\0') return (TokenType)i;
    }
    return TT_COUNT;
}

/* -- stream --------------------------------------------------------------- */

void ts_init(TokenStream *ts)
{
    ts->data = NULL;
    ts->count = 0;
    ts->cap = 0;
}

void ts_free(TokenStream *ts)
{
    free(ts->data);
    ts_init(ts);
}

void ts_push(TokenStream *ts, TokenType type, const char *lexeme, int line)
{
    if (ts->count == ts->cap) {
        size_t ncap = ts->cap ? ts->cap * 2 : 32;
        Token *nd = (Token *)realloc(ts->data, ncap * sizeof(Token));
        if (!nd) { fprintf(stderr, "out of memory\n"); exit(1); }
        ts->data = nd;
        ts->cap = ncap;
    }
    Token *t = &ts->data[ts->count++];
    t->type = type;
    t->line = line;
    snprintf(t->lexeme, MAX_LEXEME, "%s", lexeme ? lexeme : "");
}

void ts_append_eof(TokenStream *ts)
{
    if (ts->count > 0 && ts->data[ts->count - 1].type == TT_EOF) return;
    int line = ts->count > 0 ? ts->data[ts->count - 1].line : 1;
    ts_push(ts, TT_EOF, "", line);
}

/* -- plain-text token-stream format --------------------------------------- */

/* Parse one whitespace-delimited piece: TYPE or TYPE(lexeme). */
static int parse_token_spec(const char *piece, int line_no, TokenStream *out,
                            char *err, size_t errsz)
{
    char name[MAX_LEXEME];
    char lexeme[MAX_LEXEME];
    int  have_lexeme = 0;
    size_t n = 0;

    /* leading identifier: [A-Za-z_]+ */
    const char *p = piece;
    while (*p && (isalpha((unsigned char)*p) || *p == '_')) {
        if (n + 1 < sizeof name) name[n++] = *p;
        p++;
    }
    name[n] = '\0';

    if (n == 0) {
        snprintf(err, errsz,
                 "Line %d: cannot parse token spec '%s' (expected TYPE or TYPE(lexeme))",
                 line_no, piece);
        return -1;
    }

    if (*p == '(') {
        /* everything up to the LAST ')' is the lexeme, so ')' itself works:
         * RPAREN()) is not a thing, but NUM(1) and RPAREN()) both round-trip. */
        const char *close = strrchr(p, ')');
        if (!close || close[1] != '\0') {
            snprintf(err, errsz,
                     "Line %d: cannot parse token spec '%s' (expected TYPE or TYPE(lexeme))",
                     line_no, piece);
            return -1;
        }
        size_t len = (size_t)(close - (p + 1));
        if (len >= sizeof lexeme) len = sizeof lexeme - 1;
        memcpy(lexeme, p + 1, len);
        lexeme[len] = '\0';
        have_lexeme = 1;
    } else if (*p != '\0') {
        snprintf(err, errsz,
                 "Line %d: cannot parse token spec '%s' (expected TYPE or TYPE(lexeme))",
                 line_no, piece);
        return -1;
    }

    TokenType tt = token_type_from_name(name);
    if (tt == TT_COUNT) {
        size_t off = (size_t)snprintf(err, errsz,
                                      "Line %d: unknown token type '%s' in '%s'. Valid types: ",
                                      line_no, name, piece);
        for (int i = 0; i < TT_COUNT && off < errsz; i++) {
            off += (size_t)snprintf(err + off, errsz - off, "%s%s",
                                    i ? ", " : "", TYPE_NAMES[i]);
        }
        return -1;
    }

    if (!have_lexeme) {
        const char *def = DEFAULT_LEXEME[tt];
        if (def == NULL) {
            snprintf(err, errsz,
                     "Line %d: token type %s needs an explicit lexeme, e.g. %s(x) or %s(42)",
                     line_no, name, name, name);
            return -1;
        }
        snprintf(lexeme, sizeof lexeme, "%s", def);
    }

    ts_push(out, tt, lexeme, line_no);
    return 0;
}

int ts_parse_text(const char *text, TokenStream *out, char *err, size_t errsz)
{
    int line_no = 0;
    const char *line_start = text;

    err[0] = '\0';

    for (;;) {
        const char *nl = strchr(line_start, '\n');
        size_t line_len = nl ? (size_t)(nl - line_start) : strlen(line_start);
        line_no++;

        /* strip comment: everything from the first '#' */
        const char *hash = (const char *)memchr(line_start, '#', line_len);
        if (hash) line_len = (size_t)(hash - line_start);

        /* split on whitespace */
        size_t i = 0;
        while (i < line_len) {
            while (i < line_len && isspace((unsigned char)line_start[i])) i++;
            size_t start = i;
            while (i < line_len && !isspace((unsigned char)line_start[i])) i++;
            if (i > start) {
                char piece[MAX_LEXEME * 2];
                size_t plen = i - start;
                if (plen >= sizeof piece) plen = sizeof piece - 1;
                memcpy(piece, line_start + start, plen);
                piece[plen] = '\0';
                if (parse_token_spec(piece, line_no, out, err, errsz) != 0) return -1;
            }
        }

        if (!nl) break;
        line_start = nl + 1;
    }

    ts_append_eof(out);
    return 0;
}

int ts_parse_file(const char *path, TokenStream *out, char *err, size_t errsz)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(err, errsz, "cannot open '%s'", path);
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); snprintf(err, errsz, "cannot read '%s'", path); return -1; }
    long size = ftell(f);
    if (size < 0) { fclose(f); snprintf(err, errsz, "cannot read '%s'", path); return -1; }
    rewind(f);

    char *buf = (char *)malloc((size_t)size + 1);
    if (!buf) { fclose(f); snprintf(err, errsz, "out of memory reading '%s'", path); return -1; }
    size_t got = fread(buf, 1, (size_t)size, f);
    buf[got] = '\0';
    fclose(f);

    int rc = ts_parse_text(buf, out, err, errsz);
    free(buf);
    return rc;
}
