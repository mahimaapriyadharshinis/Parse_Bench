/* Token representation for the syntax analyzer.
 *
 * The lexer is out of scope for this project (the analyzer is handed a token
 * stream). This header defines the Token type, the plain-text token-stream
 * format reader, and a few hand-built streams used for demos and tests.
 */
#ifndef TOKEN_H
#define TOKEN_H

#include <stddef.h>

/* Keep TT_EOF last before TT_COUNT: terminal sets are bitsets over this
 * enum, and grammar.h assumes TT_COUNT fits in a uint32_t with one spare
 * bit left over for epsilon. */
typedef enum {
    /* keywords */
    TT_INT, TT_IF, TT_ELSE, TT_WHILE, TT_PRINT,
    /* literals / identifiers */
    TT_ID, TT_NUM,
    /* operators */
    TT_ASSIGN, TT_PLUS, TT_MINUS, TT_STAR, TT_SLASH,
    TT_LT, TT_GT, TT_LE, TT_GE, TT_EQ, TT_NE,
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

#endif /* TOKEN_H */
