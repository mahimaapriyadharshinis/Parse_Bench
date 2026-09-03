/* Built-in demo token streams, written in the plain-text token-stream format
 * so the samples double as documentation of that format. Line breaks matter:
 * each source line becomes a line number in error messages. */
#ifndef SAMPLES_H
#define SAMPLES_H

typedef struct {
    const char *name;
    const char *description;
    const char *text;
} Sample;

/* int x;
 * x = 1 + 2 * 3;
 * print(x); */
#define SAMPLE_VALID_TEXT                            \
    "INT ID(x) SEMI\n"                               \
    "ID(x) ASSIGN NUM(1) PLUS NUM(2) STAR NUM(3) SEMI\n" \
    "PRINT LPAREN ID(x) RPAREN SEMI\n"

/* if (x < 10) { x = x + 1; } else { x = 0; } */
#define SAMPLE_VALID_IF_TEXT                         \
    "IF LPAREN ID(x) LT NUM(10) RPAREN LBRACE\n"     \
    "ID(x) ASSIGN ID(x) PLUS NUM(1) SEMI\n"          \
    "RBRACE ELSE LBRACE\n"                           \
    "ID(x) ASSIGN NUM(0) SEMI\n"                     \
    "RBRACE\n"

/* int x;
 * x = 1 + 2        <- missing ';'
 * print(x;         <- missing ')' */
#define SAMPLE_INVALID_TEXT                          \
    "INT ID(x) SEMI\n"                               \
    "ID(x) ASSIGN NUM(1) PLUS NUM(2)\n"              \
    "PRINT LPAREN ID(x) SEMI\n"

#define SAMPLE_EMPTY_TEXT ""

static const Sample SAMPLES[] = {
    { "valid",    "int x; x = 1 + 2 * 3; print(x);",              SAMPLE_VALID_TEXT },
    { "valid_if", "if (x < 10) { x = x + 1; } else { x = 0; }",   SAMPLE_VALID_IF_TEXT },
    { "invalid",  "two independent syntax errors, both recovered", SAMPLE_INVALID_TEXT },
    { "empty",    "an empty token stream",                        SAMPLE_EMPTY_TEXT },
};

#define SAMPLE_COUNT ((int)(sizeof SAMPLES / sizeof SAMPLES[0]))

#endif /* SAMPLES_H */
