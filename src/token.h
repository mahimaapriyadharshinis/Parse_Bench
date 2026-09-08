/* Token representation for the syntax analyzer.
 *
 * The analyzer itself only ever consumes a TokenStream -- it never reads
 * source text. This header defines the Token type, the plain-text
 * token-stream format reader/writer, and a few hand-built streams used for
 * demos and tests. (An optional convenience lexer that produces a
 * TokenStream from real C-like source lives separately, in src/lexer.h --
 * it sits in front of this interface, not inside it.)
 */
#ifndef TOKEN_H
#define TOKEN_H

#include <stddef.h>
#include <stdio.h>

/* Keep TT_EOF last before TT_COUNT: terminal sets are bitsets over this
 * enum, and grammar.h assumes TT_COUNT fits in a uint32_t with one spare
 * bit left over for epsilon. */
typedef enum {
    /* keywords */
    TT_INT, TT_IF, TT_ELSE, TT_WHILE, TT_PRINT, TT_FOR, TT_BREAK, TT_CONTINUE,
    /* literals / identifiers */
    TT_ID, TT_NUM,
    /* operators */
    TT_ASSIGN, TT_PLUS, TT_MINUS, TT_STAR, TT_SLASH,
    TT_LT, TT_GT, TT_LE, TT_GE, TT_EQ, TT_NE,
    TT_AND, TT_OR, TT_NOT,
    /* punctuation */
    TT_LPAREN, TT_RPAREN, TT_LBRACE, TT_RBRACE, TT_SEMI,
    /* end of stream */
    TT_EOF,
    TT_COUNT
} TokenType;

#define MAX_LEXEME 64

typedef struct {
    TokenType type;
    char      lexeme[MAX_LEXEME];
    int       line;
} Token;

/* A growable token stream. */
typedef struct {
    Token *data;
    size_t count;
    size_t cap;
} TokenStream;

const char *token_type_name(TokenType t);

/* Fixed spelling for token types that have one ("int", ";", "+"...);
 * NULL for ID/NUM/EOF, whose lexeme must be supplied explicitly. */
const char *token_default_lexeme(TokenType t);

/* Look up a TokenType by name, case-insensitively. Returns TT_COUNT if the
 * name isn't a valid token type. */
TokenType token_type_from_name(const char *name);

void ts_init(TokenStream *ts);
void ts_free(TokenStream *ts);
void ts_push(TokenStream *ts, TokenType type, const char *lexeme, int line);
/* Append an EOF token unless the stream already ends with one. */
void ts_append_eof(TokenStream *ts);

/* Parse the plain-text token-stream format into `out`.
 *
 *   One or more tokens per line, whitespace-separated, '#' starts a comment.
 *   Each token is written as TYPE or TYPE(lexeme). TYPE must be a TokenType
 *   name (case-insensitive). The lexeme is optional for token types with a
 *   fixed spelling but REQUIRED for ID and NUM. Each source line becomes one
 *   line number in error messages.
 *
 * Returns 0 on success; on failure returns -1 and writes a human-readable
 * reason into `err`. */
int ts_parse_text(const char *text, TokenStream *out, char *err, size_t errsz);
int ts_parse_file(const char *path, TokenStream *out, char *err, size_t errsz);

/* Write `ts` back out in the plain-text token-stream format above (the
 * inverse of ts_parse_text) -- one line per distinct source `line` value,
 * each token as TYPE or TYPE(lexeme). A trailing EOF token, if present, is
 * not printed; ts_parse_text/ts_append_eof restore it on read. */
void ts_write_text(const TokenStream *ts, FILE *out);

#endif /* TOKEN_H */
