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

/* int sum;
 * int i;
 * sum = 0;
 * for (i = 0; i < 10 && sum < 100; i = i + 1) {
 *     if (i == 5) { continue; }
 *     if (sum > 50 || i == 9) { break; }
 *     sum = sum + i;
 * }
 * if (!i < 3) { print(sum); } */
#define SAMPLE_CONTROL_FLOW_TEXT                                              \
    "INT ID(sum) SEMI\n"                                                     \
    "INT ID(i) SEMI\n"                                                       \
    "ID(sum) ASSIGN NUM(0) SEMI\n"                                           \
    "FOR LPAREN ID(i) ASSIGN NUM(0) SEMI ID(i) LT NUM(10) AND ID(sum) LT NUM(100) SEMI "  \
        "ID(i) ASSIGN ID(i) PLUS NUM(1) RPAREN LBRACE\n"                     \
    "    IF LPAREN ID(i) EQ NUM(5) RPAREN LBRACE\n"                          \
    "        CONTINUE SEMI\n"                                                \
    "    RBRACE\n"                                                           \
    "    IF LPAREN ID(sum) GT NUM(50) OR ID(i) EQ NUM(9) RPAREN LBRACE\n"     \
    "        BREAK SEMI\n"                                                   \
    "    RBRACE\n"                                                           \
    "    ID(sum) ASSIGN ID(sum) PLUS ID(i) SEMI\n"                           \
    "RBRACE\n"                                                               \
    "IF LPAREN NOT ID(i) LT NUM(3) RPAREN LBRACE\n"                          \
    "    PRINT LPAREN ID(sum) RPAREN SEMI\n"                                 \
    "RBRACE\n"

static const Sample SAMPLES[] = {
    { "valid",    "int x; x = 1 + 2 * 3; print(x);",              SAMPLE_VALID_TEXT },
    { "valid_if", "if (x < 10) { x = x + 1; } else { x = 0; }",   SAMPLE_VALID_IF_TEXT },
    { "control_flow", "for/break/continue and && || ! conditions", SAMPLE_CONTROL_FLOW_TEXT },
    { "invalid",  "two independent syntax errors, both recovered", SAMPLE_INVALID_TEXT },
    { "empty",    "an empty token stream",                        SAMPLE_EMPTY_TEXT },
};

#define SAMPLE_COUNT ((int)(sizeof SAMPLES / sizeof SAMPLES[0]))

#endif /* SAMPLES_H */
