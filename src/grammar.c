#include "grammar.h"

#include <stdio.h>
#include <string.h>

#define T(x)  ((Symbol)(x))
#define N(x)  NT_SYM(x)
#define EPS   SYM_EPSILON

/* Productions in the same order as the grammar is written out in grammar.h,
 * so the LL(1) table and any dump of it read the way the grammar reads. */
const Production GRAMMAR[] = {
    { NT_PROGRAM,    { N(NT_STATEMENT), N(NT_PROGRAM) },                             2 },
    { NT_PROGRAM,    { EPS },                                                        1 },

    { NT_STATEMENT,  { N(NT_DECLSTMT) },                                             1 },
    { NT_STATEMENT,  { N(NT_ASSIGNSTMT) },                                           1 },
    { NT_STATEMENT,  { N(NT_IFSTMT) },                                               1 },
    { NT_STATEMENT,  { N(NT_WHILESTMT) },                                            1 },
    { NT_STATEMENT,  { N(NT_BLOCK) },                                                1 },
    { NT_STATEMENT,  { N(NT_PRINTSTMT) },                                            1 },

    { NT_DECLSTMT,   { T(TT_INT), T(TT_ID), T(TT_SEMI) },                            3 },

    { NT_ASSIGNSTMT, { T(TT_ID), T(TT_ASSIGN), N(NT_EXPR), T(TT_SEMI) },             4 },

    { NT_IFSTMT,     { T(TT_IF), T(TT_LPAREN), N(NT_COND), T(TT_RPAREN),
                       N(NT_BLOCK), N(NT_ELSEPART) },                                6 },

    { NT_ELSEPART,   { T(TT_ELSE), N(NT_BLOCK) },                                    2 },
    { NT_ELSEPART,   { EPS },                                                        1 },

    { NT_WHILESTMT,  { T(TT_WHILE), T(TT_LPAREN), N(NT_COND), T(TT_RPAREN),
                       N(NT_BLOCK) },                                                5 },

    { NT_BLOCK,      { T(TT_LBRACE), N(NT_STMTLIST), T(TT_RBRACE) },                 3 },

    { NT_STMTLIST,   { N(NT_STATEMENT), N(NT_STMTLIST) },                            2 },
    { NT_STMTLIST,   { EPS },                                                        1 },

    { NT_PRINTSTMT,  { T(TT_PRINT), T(TT_LPAREN), N(NT_EXPR), T(TT_RPAREN),
                       T(TT_SEMI) },                                                 5 },

    { NT_COND,       { N(NT_EXPR), N(NT_RELOP), N(NT_EXPR) },                        3 },

    { NT_RELOP,      { T(TT_LT) },                                                   1 },
    { NT_RELOP,      { T(TT_GT) },                                                   1 },
    { NT_RELOP,      { T(TT_LE) },                                                   1 },
    { NT_RELOP,      { T(TT_GE) },                                                   1 },
    { NT_RELOP,      { T(TT_EQ) },                                                   1 },
    { NT_RELOP,      { T(TT_NE) },                                                   1 },

    { NT_EXPR,       { N(NT_TERM), N(NT_EXPRTAIL) },                                 2 },

    { NT_EXPRTAIL,   { T(TT_PLUS),  N(NT_TERM), N(NT_EXPRTAIL) },                    3 },
    { NT_EXPRTAIL,   { T(TT_MINUS), N(NT_TERM), N(NT_EXPRTAIL) },                    3 },
    { NT_EXPRTAIL,   { EPS },                                                        1 },

    { NT_TERM,       { N(NT_FACTOR), N(NT_TERMTAIL) },                               2 },

    { NT_TERMTAIL,   { T(TT_STAR),  N(NT_FACTOR), N(NT_TERMTAIL) },                  3 },
    { NT_TERMTAIL,   { T(TT_SLASH), N(NT_FACTOR), N(NT_TERMTAIL) },                  3 },
    { NT_TERMTAIL,   { EPS },                                                        1 },

    { NT_FACTOR,     { T(TT_ID) },                                                   1 },
    { NT_FACTOR,     { T(TT_NUM) },                                                  1 },
    { NT_FACTOR,     { T(TT_LPAREN), N(NT_EXPR), T(TT_RPAREN) },                     3 },
};

const int GRAMMAR_COUNT = (int)(sizeof GRAMMAR / sizeof GRAMMAR[0]);

static const char *const NT_NAMES[NT_COUNT] = {
    "program", "statement", "declStmt", "assignStmt", "ifStmt",
    "elsePart", "whileStmt", "block", "stmtList", "printStmt",
    "cond", "relop", "expr", "exprTail", "term", "termTail",
    "factor"
};

TermSet g_first[NT_COUNT];
TermSet g_follow[NT_COUNT];
int     g_table[NT_COUNT][TT_COUNT];

const char *nonterm_name(NonTerm nt)
{
    return (nt >= 0 && nt < NT_COUNT) ? NT_NAMES[nt] : "?";
}

/* FIRST of a symbol sequence, using the FIRST sets computed so far. This is
 * the one helper all three algorithms below share (the Python original
 * defines it three times as a nested function; here it is written once). */
static TermSet first_of_sequence(const Symbol *seq, int len)
{
    TermSet result = 0;
    for (int i = 0; i < len; i++) {
        Symbol sym = seq[i];
        if (sym == SYM_EPSILON) return result | TS_EPSILON;
        if (SYM_IS_TERMINAL(sym)) return result | TS_BIT(sym);
        TermSet sym_first = g_first[SYM_NT(sym)];
        result |= (sym_first & ~TS_EPSILON);
        if (!(sym_first & TS_EPSILON)) return result;
    }
    return result | TS_EPSILON;
}

/* Standard fixed-point FIRST-set algorithm. */
static void compute_first_sets(void)
{
    memset(g_first, 0, sizeof g_first);

    int changed = 1;
    while (changed) {
        changed = 0;
        for (int p = 0; p < GRAMMAR_COUNT; p++) {
            const Production *prod = &GRAMMAR[p];
            TermSet before = g_first[prod->lhs];
            g_first[prod->lhs] |= first_of_sequence(prod->rhs, prod->len);
            if (g_first[prod->lhs] != before) changed = 1;
        }
    }
}

/* Standard fixed-point FOLLOW-set algorithm. */
static void compute_follow_sets(void)
{
    memset(g_follow, 0, sizeof g_follow);
    g_follow[GRAMMAR_START] |= TS_BIT(TT_EOF);   /* end-of-input marker */

    int changed = 1;
    while (changed) {
        changed = 0;
        for (int p = 0; p < GRAMMAR_COUNT; p++) {
            const Production *prod = &GRAMMAR[p];
            for (int i = 0; i < prod->len; i++) {
                Symbol sym = prod->rhs[i];
                if (!SYM_IS_NONTERM(sym)) continue;  /* skips terminals and epsilon */
                NonTerm target = SYM_NT(sym);

                int rest_len = prod->len - (i + 1);
                TermSet rest_first = (rest_len > 0)
                    ? first_of_sequence(&prod->rhs[i + 1], rest_len)
                    : TS_EPSILON;

                TermSet before = g_follow[target];
                g_follow[target] |= (rest_first & ~TS_EPSILON);
                if (rest_first & TS_EPSILON) g_follow[target] |= g_follow[prod->lhs];
                if (g_follow[target] != before) changed = 1;
            }
        }
    }
}

/* Build the LL(1) parsing table: (non-terminal, terminal) -> production.
 *
 * Also serves as a correctness check: a second production ever trying to
 * overwrite an existing table cell means the grammar is *not* LL(1) (a
 * FIRST/FIRST or FIRST/FOLLOW conflict). That is reported immediately, so a
 * grammar bug is caught at startup rather than causing silent mis-parses. */
static int build_ll1_table(char *err, size_t errsz)
{
    for (int nt = 0; nt < NT_COUNT; nt++)
        for (int t = 0; t < TT_COUNT; t++)
            g_table[nt][t] = -1;

    for (int p = 0; p < GRAMMAR_COUNT; p++) {
        const Production *prod = &GRAMMAR[p];
        TermSet prod_first = first_of_sequence(prod->rhs, prod->len);

        /* the terminals this production can start with... */
        TermSet cells = prod_first & ~TS_EPSILON;
        /* ...plus, if it can vanish, everything that can follow the LHS */
        if (prod_first & TS_EPSILON) cells |= g_follow[prod->lhs];

        for (int t = 0; t < TT_COUNT; t++) {
            if (!TS_HAS(cells, t)) continue;
            int existing = g_table[prod->lhs][t];
            if (existing != -1) {
                char a[128], b[128];
                production_rhs_str(&GRAMMAR[existing], a, sizeof a);
                production_rhs_str(prod, b, sizeof b);
                snprintf(err, errsz,
                         "Grammar is not LL(1): conflict at (%s, %s) between "
                         "%s -> %s and %s -> %s",
                         nonterm_name(prod->lhs), token_type_name((TokenType)t),
                         nonterm_name(prod->lhs), a,
                         nonterm_name(prod->lhs), b);
                return -1;
            }
            g_table[prod->lhs][t] = p;
        }
    }
    return 0;
}

int grammar_init(char *err, size_t errsz)
{
    err[0] = '\0';
    compute_first_sets();
    compute_follow_sets();
    return build_ll1_table(err, errsz);
}

int grammar_table_entries(void)
{
    int n = 0;
    for (int nt = 0; nt < NT_COUNT; nt++)
        for (int t = 0; t < TT_COUNT; t++)
            if (g_table[nt][t] != -1) n++;
    return n;
}

void production_rhs_str(const Production *p, char *buf, size_t bufsz)
{
    size_t off = 0;
    buf[0] = '\0';
    for (int i = 0; i < p->len && off < bufsz; i++) {
        const char *s;
        if (p->rhs[i] == SYM_EPSILON) {
            s = "\xce\xb5";                      /* UTF-8 epsilon */
        } else if (SYM_IS_TERMINAL(p->rhs[i])) {
            s = token_default_lexeme((TokenType)p->rhs[i]);
            if (!s || !*s) s = token_type_name((TokenType)p->rhs[i]);
        } else {
            s = nonterm_name(SYM_NT(p->rhs[i]));
        }
        off += (size_t)snprintf(buf + off, bufsz - off, "%s%s", i ? " " : "", s);
    }
}

void termset_str(TermSet s, char *buf, size_t bufsz)
{
    size_t off = 0;
    int first = 1;
    buf[0] = '\0';
    for (int t = 0; t < TT_COUNT && off < bufsz; t++) {
        if (!TS_HAS(s, t)) continue;
        const char *name = token_default_lexeme((TokenType)t);
        if (!name || !*name) name = token_type_name((TokenType)t);
        off += (size_t)snprintf(buf + off, bufsz - off, "%s%s", first ? "" : ", ", name);
        first = 0;
    }
    if ((s & TS_EPSILON) && off < bufsz)
        snprintf(buf + off, bufsz - off, "%s\xce\xb5", first ? "" : ", ");
}
