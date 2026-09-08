#include "lexer.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *p;      /* current position in src */
    int         line;
    TokenStream *out;
} Lexer;

static int peek(Lexer *lx) { return *lx->p; }
static int peek2(Lexer *lx) { return lx->p[0] ? lx->p[1] : '\0'; }

static int advance(Lexer *lx)
{
    int c = *lx->p++;
    if (c == '\n') lx->line++;
    return c;
}

/* Skip whitespace and comments (line and block style). Returns 0 on
 * success, -1 if a block comment is never closed. */
static int skip_trivia(Lexer *lx, char *err, size_t errsz)
{
    for (;;) {
        int c = peek(lx);
        if (c == '\0') return 0;

        if (isspace((unsigned char)c)) { advance(lx); continue; }

        if (c == '/' && peek2(lx) == '/') {
            while (peek(lx) != '\0' && peek(lx) != '\n') advance(lx);
            continue;
        }

        if (c == '/' && peek2(lx) == '*') {
            int start_line = lx->line;
            advance(lx); advance(lx);
            while (peek(lx) != '\0' && !(peek(lx) == '*' && peek2(lx) == '/')) advance(lx);
            if (peek(lx) == '\0') {
                snprintf(err, errsz, "Line %d: unterminated block comment", start_line);
                return -1;
            }
            advance(lx); advance(lx); /* consume the closing delimiter */
            continue;
        }

        return 0;
    }
}

static TokenType keyword_type(const char *word)
{
    static const struct { const char *word; TokenType type; } KEYWORDS[] = {
        {"int", TT_INT}, {"if", TT_IF}, {"else", TT_ELSE},
        {"while", TT_WHILE}, {"print", TT_PRINT}, {"for", TT_FOR},
        {"break", TT_BREAK}, {"continue", TT_CONTINUE},
    };
    for (size_t i = 0; i < sizeof KEYWORDS / sizeof KEYWORDS[0]; i++)
        if (strcmp(word, KEYWORDS[i].word) == 0) return KEYWORDS[i].type;
    return TT_ID;
}

/* One- or two-character operator/punctuation starting at the current
 * position. `second` (if non-zero) extends the token when it matches the
 * next character, consuming it too. */
static int lex_operator(Lexer *lx, char *err, size_t errsz)
{
    int line = lx->line;
    int c = advance(lx);

    switch (c) {
        case '+': ts_push(lx->out, TT_PLUS, "+", line); return 0;
        case '-': ts_push(lx->out, TT_MINUS, "-", line); return 0;
        case '*': ts_push(lx->out, TT_STAR, "*", line); return 0;
        case '/': ts_push(lx->out, TT_SLASH, "/", line); return 0;
        case '(': ts_push(lx->out, TT_LPAREN, "(", line); return 0;
        case ')': ts_push(lx->out, TT_RPAREN, ")", line); return 0;
        case '{': ts_push(lx->out, TT_LBRACE, "{", line); return 0;
        case '}': ts_push(lx->out, TT_RBRACE, "}", line); return 0;
        case ';': ts_push(lx->out, TT_SEMI, ";", line); return 0;
        case '=':
            if (peek(lx) == '=') { advance(lx); ts_push(lx->out, TT_EQ, "==", line); }
            else ts_push(lx->out, TT_ASSIGN, "=", line);
            return 0;
        case '<':
            if (peek(lx) == '=') { advance(lx); ts_push(lx->out, TT_LE, "<=", line); }
            else ts_push(lx->out, TT_LT, "<", line);
            return 0;
        case '>':
            if (peek(lx) == '=') { advance(lx); ts_push(lx->out, TT_GE, ">=", line); }
            else ts_push(lx->out, TT_GT, ">", line);
            return 0;
        case '!':
            if (peek(lx) == '=') { advance(lx); ts_push(lx->out, TT_NE, "!=", line); }
            else ts_push(lx->out, TT_NOT, "!", line);
            return 0;
        case '&':
            if (peek(lx) == '&') { advance(lx); ts_push(lx->out, TT_AND, "&&", line); return 0; }
            snprintf(err, errsz, "Line %d: '&' must be followed by '&' (no bitwise '&' in this grammar)", line);
            return -1;
        case '|':
            if (peek(lx) == '|') { advance(lx); ts_push(lx->out, TT_OR, "||", line); return 0; }
            snprintf(err, errsz, "Line %d: '|' must be followed by '|' (no bitwise '|' in this grammar)", line);
            return -1;
        default:
            if (isprint((unsigned char)c))
                snprintf(err, errsz, "Line %d: unexpected character '%c'", line, c);
            else
                snprintf(err, errsz, "Line %d: unexpected byte 0x%02x", line, (unsigned char)c);
            return -1;
    }
}

int lex_source(const char *src, TokenStream *out, char *err, size_t errsz)
{
    Lexer lx = { .p = src, .line = 1, .out = out };
    err[0] = '\0';

    for (;;) {
        if (skip_trivia(&lx, err, errsz) != 0) return -1;
        int c = peek(&lx);
        if (c == '\0') break;
        int line = lx.line;

        if (isdigit((unsigned char)c)) {
            char lexeme[MAX_LEXEME];
            size_t len = 0;
            while (isdigit((unsigned char)peek(&lx))) {
                if (len + 1 >= sizeof lexeme) {
                    snprintf(err, errsz, "Line %d: number literal too long (max %d digits)",
                             line, (int)sizeof lexeme - 1);
                    return -1;
                }
                lexeme[len++] = (char)advance(&lx);
            }
            lexeme[len] = '\0';
            ts_push(out, TT_NUM, lexeme, line);
            continue;
        }

        if (isalpha((unsigned char)c) || c == '_') {
            char lexeme[MAX_LEXEME];
            size_t len = 0;
            while (isalnum((unsigned char)peek(&lx)) || peek(&lx) == '_') {
                if (len + 1 >= sizeof lexeme) {
                    snprintf(err, errsz, "Line %d: identifier too long (max %d characters)",
                             line, (int)sizeof lexeme - 1);
                    return -1;
                }
                lexeme[len++] = (char)advance(&lx);
            }
            lexeme[len] = '\0';
            ts_push(out, keyword_type(lexeme), lexeme, line);
            continue;
        }

        if (lex_operator(&lx, err, errsz) != 0) return -1;
    }

    ts_append_eof(out);
    return 0;
}
