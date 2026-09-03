/* Grammar definition (pure BNF, no left recursion) plus generic FIRST/FOLLOW/
 * LL(1)-table computation.
 *
 * The restricted language grammar, in EBNF, is:
 *
 *     program     -> statement*
 *     statement   -> declStmt | assignStmt | ifStmt | whileStmt | block | printStmt
 *     declStmt    -> "int" ID ";"
 *     assignStmt  -> ID "=" expr ";"
 *     ifStmt      -> "if" "(" cond ")" block ( "else" block )?
 *     whileStmt   -> "while" "(" cond ")" block
 *     block       -> "{" statement* "}"
 *     printStmt   -> "print" "(" expr ")" ";"
 *     cond        -> expr relop expr
 *     relop       -> "<" | ">" | "<=" | ">=" | "==" | "!="
 *     expr        -> term (("+"|"-") term)*
 *     term        -> factor (("*"|"/") factor)*
 *     factor      -> ID | NUM | "(" expr ")"
 *
 * GRAMMAR below is the same grammar rewritten in *pure* BNF (the `*`/`?`
 * repetition operators expanded into right-recursive rules with an explicit
 * epsilon production) so that a textbook FIRST/FOLLOW/LL(1)-table algorithm
 * can be run over it mechanically, exactly as covered in class.
 */
#ifndef GRAMMAR_H
#define GRAMMAR_H

#include <stdint.h>

#include "token.h"

typedef enum {
    NT_PROGRAM, NT_STATEMENT, NT_DECLSTMT, NT_ASSIGNSTMT, NT_IFSTMT,
    NT_ELSEPART, NT_WHILESTMT, NT_BLOCK, NT_STMTLIST, NT_PRINTSTMT,
    NT_COND, NT_RELOP, NT_EXPR, NT_EXPRTAIL, NT_TERM, NT_TERMTAIL,
    NT_FACTOR,
    NT_COUNT
} NonTerm;

#define GRAMMAR_START NT_PROGRAM

/* A grammar symbol is encoded in a single int: values below TT_COUNT are
 * terminals (TokenType), NT_OFFSET+n is non-terminal n, and SYM_EPSILON
 * marks the empty production. */
#define NT_OFFSET   100
#define SYM_EPSILON 999
typedef int Symbol;

#define SYM_IS_TERMINAL(s) ((s) >= 0 && (s) < TT_COUNT)
#define SYM_IS_NONTERM(s)  ((s) >= NT_OFFSET && (s) < NT_OFFSET + NT_COUNT)
#define SYM_NT(s)          ((NonTerm)((s) - NT_OFFSET))
#define NT_SYM(n)          ((Symbol)(NT_OFFSET + (n)))

#define MAX_RHS 8

typedef struct {
    NonTerm lhs;
    Symbol  rhs[MAX_RHS];
    int     len;        /* number of symbols; an epsilon production is len 1 */
} Production;

/* A set of terminals, as a bitset. Bit TT_COUNT is the epsilon flag, which
 * is why TT_COUNT must stay below 32. */
typedef uint32_t TermSet;
#define TS_BIT(t)   ((TermSet)1u << (t))
#define TS_EPSILON  TS_BIT(TT_COUNT)
#define TS_HAS(s,t) (((s) & TS_BIT(t)) != 0)

extern const Production GRAMMAR[];
extern const int        GRAMMAR_COUNT;

/* Computed by grammar_init(). */
extern TermSet g_first[NT_COUNT];
extern TermSet g_follow[NT_COUNT];
/* g_table[nt][terminal] is an index into GRAMMAR, or -1 for a blank cell. */
extern int     g_table[NT_COUNT][TT_COUNT];

/* Run the FIRST/FOLLOW/LL(1)-table algorithms. Returns 0 on success. A
 * non-zero return means the grammar is NOT LL(1): a FIRST/FIRST or
 * FIRST/FOLLOW conflict was found, and `err` describes it. Building the
 * table is therefore also the proof that the grammar is LL(1). */
int grammar_init(char *err, size_t errsz);

const char *nonterm_name(NonTerm nt);
/* Number of filled cells in the LL(1) table. */
int grammar_table_entries(void);
/* Render a production's right-hand side, e.g. "statement program" or "ε". */
void production_rhs_str(const Production *p, char *buf, size_t bufsz);
/* Render a terminal set, e.g. "int, if, while, ε". */
void termset_str(TermSet s, char *buf, size_t bufsz);

#endif /* GRAMMAR_H */
