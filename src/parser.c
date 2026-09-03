/* Recursive-descent LL(1) parser with parse-tree construction and combined
 * panic-mode / phrase-level syntax error recovery.
 *
 * Each parse_* function below corresponds 1:1 to a non-terminal in
 * grammar.c's GRAMMAR. Two kinds of error handling are used, matching the
 * two classic recovery strategies:
 *
 * * "Soft" expect (`expect`) -- used for an expected closing/terminating
 *   token (e.g. a missing ';' or ')'). We report the error but do NOT
 *   unwind: we pretend the token was there (phrase-level recovery) and keep
 *   parsing from the current token, since the rest of the input is often
 *   still valid.
 *
 * * "Hard" dispatch failure (`fail`) -- used when the current token doesn't
 *   match ANY alternative of a non-terminal (e.g. a statement that starts
 *   with a token that isn't int/id/if/while/{/print). There's no sensible
 *   partial node to build, so we abandon the current statement and let
 *   parse_stmt_sequence catch it and run synchronize() (panic-mode
 *   recovery): skip tokens until one in FIRST(statement) is found, then
 *   resume.
 *
 * The Python original raised a ParseError exception for the hard case. C
 * has no exceptions, so `fail` does a longjmp back to the setjmp in
 * parse_stmt_sequence -- the same non-local unwind, spelled out. That
 * abandons any half-built subtree, which is exactly why parse-tree nodes
 * come from an arena instead of being individually owned and freed.
 */
#include "parser.h"

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "grammar.h"

/* Human-readable spellings for token types, used to phrase error messages
 * the way a person would say them out loud rather than as raw enum names. */
const char *friendly_type(TokenType t)
{
    switch (t) {
    case TT_SEMI:   return "';'";
    case TT_RPAREN: return "')'";
    case TT_LPAREN: return "'('";
    case TT_RBRACE: return "'}'";
    case TT_LBRACE: return "'{'";
    case TT_ASSIGN: return "'='";
    case TT_ID:     return "an identifier";
    case TT_NUM:    return "a number";
    case TT_INT:    return "'int'";
    case TT_ELSE:   return "'else'";
    case TT_EOF:    return "the end of the file";
    default:        return token_type_name(t);
    }
}

static void friendly_token(const Token *t, char *buf, size_t bufsz)
{
    if (t->type == TT_EOF) snprintf(buf, bufsz, "the end of the file");
    else                   snprintf(buf, bufsz, "'%s'", t->lexeme);
}

/* -- parser state --------------------------------------------------------- */

#define MAX_ERRORS 256

/* One landing pad per active parse_stmt_sequence, chained innermost-first.
 * fail() jumps to `top`, so an error is always caught by the nearest
 * enclosing statement sequence -- the same nesting Python got from
 * try/except. Each pad lives in its owner's stack frame, so the chain
 * unwinds for free. */
typedef struct Landing {
    jmp_buf         jb;
    struct Landing *prev;
} Landing;

typedef struct {
    const Token *tokens;
    size_t       count;
    size_t       pos;

    Arena *arena;
    char  *errors[MAX_ERRORS];
    int    error_count;

    TraceStep *trace;
    int        trace_count;
    int        trace_cap;

    /* Set by fail() immediately before the longjmp, then consumed on the
     * setjmp side -- the C stand-in for the exception object Python would
     * have thrown. */
    Landing *top;
    char     pending_error[MAX_ERROR_MSG];
} Parser;

static TermSet first_statement(void)
{
    return g_first[NT_STATEMENT] & ~TS_EPSILON;
}

/* -- token stream helpers -------------------------------------------------- */

static const Token *current(Parser *p)
{
    return &p->tokens[p->pos];
}

static int check(Parser *p, TokenType t)
{
    return current(p)->type == t;
}

static const Token *advance(Parser *p)
{
    const Token *t = &p->tokens[p->pos];
    if (t->type != TT_EOF) p->pos++;
    return t;
}

/* Record "the tree had this many nodes when the parser was at this token",
 * so the visualizer can scrub through the parse. */
static void trace_mark(Parser *p, int step)
{
    if (p->trace_count == p->trace_cap) {
        int ncap = p->trace_cap ? p->trace_cap * 2 : 128;
        TraceStep *nt = (TraceStep *)realloc(p->trace, (size_t)ncap * sizeof(TraceStep));
        if (!nt) { fprintf(stderr, "out of memory\n"); exit(1); }
        p->trace = nt;
        p->trace_cap = ncap;
    }
    p->trace[p->trace_count].step = step;
    p->trace[p->trace_count].token_pos = p->pos;
    p->trace_count++;
}

static ParseNode *node(Parser *p, const char *label, int is_error)
{
    ParseNode *n = pt_node(p->arena, label, is_error);
    trace_mark(p, n->step);
    return n;
}

static ParseNode *add(Parser *p, ParseNode *parent, ParseNode *child)
{
    return pt_add(p->arena, parent, child);
}

static ParseNode *leaf(Parser *p, const Token *t)
{
    return node(p, t->lexeme[0] ? t->lexeme : token_type_name(t->type), 0);
}

static void report(Parser *p, const char *msg)
{
    if (p->error_count >= MAX_ERRORS) return;
    p->errors[p->error_count++] = arena_strdup(p->arena, msg);
}

/* Phrasing for a soft expect() mismatch. */
static void msg_missing(const Token *t, TokenType missing, char *buf, size_t bufsz)
{
    char found[MAX_LEXEME + 8];
    friendly_token(t, found, sizeof found);
    snprintf(buf, bufsz, "Line %d: missing %s - found %s instead",
             t->line, friendly_type(missing), found);
}

/* Phrasing for a hard dispatch failure, where `expected` is already a full
 * human phrase ("start of statement", "identifier, number, or '('"). */
static void msg_expected(const Token *t, const char *expected, char *buf, size_t bufsz)
{
    char found[MAX_LEXEME + 8];
    friendly_token(t, found, sizeof found);
    snprintf(buf, bufsz, "Line %d: expected %s, but found %s",
             t->line, expected, found);
}

/* Hard failure: unwind to the nearest parse_stmt_sequence, which will report
 * the error and run panic-mode recovery. */
static void fail(Parser *p, const char *expected)
{
    msg_expected(current(p), expected, p->pending_error, sizeof p->pending_error);
    if (!p->top) {   /* unreachable: every parse_statement runs under a pad */
        fprintf(stderr, "internal error: %s\n", p->pending_error);
        exit(1);
    }
    longjmp(p->top->jb, 1);
}

/* Soft expect: phrase-level recovery on mismatch (see file header). */
static ParseNode *expect(Parser *p, TokenType t)
{
    if (check(p, t)) return leaf(p, advance(p));

    char msg[MAX_ERROR_MSG];
    msg_missing(current(p), t, msg, sizeof msg);
    report(p, msg);

    char label[64];
    snprintf(label, sizeof label, "<missing %s>", token_type_name(t));
    return node(p, label, 1);
}

/* Panic-mode recovery: skip tokens until one in FIRST(statement) is found,
 * or SEMI (consumed), or an RBRACE that actually matches the block this call
 * is inside (i.e. equals `terminator`). An RBRACE that *isn't* the active
 * terminator is a stray/unmatched '}' with nothing open to close -- it must
 * be skipped like any other garbage token, not treated as a safe stopping
 * point, or a top-level stray '}' would never be consumed and this would
 * loop forever. */
static void synchronize(Parser *p, TokenType terminator)
{
    TermSet starters = first_statement();

    while (!check(p, TT_EOF)) {
        if (check(p, TT_SEMI)) { advance(p); return; }
        if (check(p, TT_RBRACE)) {
            if (current(p)->type == terminator) return;
            advance(p);
            continue;
        }
        if (TS_HAS(starters, current(p)->type)) return;
        advance(p);
    }
}

/* -- forward declarations (one per non-terminal) --------------------------- */

static void       parse_stmt_sequence(Parser *p, ParseNode *n, TokenType terminator);
static ParseNode *parse_statement(Parser *p);
static ParseNode *parse_decl_stmt(Parser *p);
static ParseNode *parse_assign_stmt(Parser *p);
static ParseNode *parse_if_stmt(Parser *p);
static ParseNode *parse_else_part(Parser *p);
static ParseNode *parse_while_stmt(Parser *p);
static ParseNode *parse_block(Parser *p);
static ParseNode *parse_print_stmt(Parser *p);
static ParseNode *parse_cond(Parser *p);
static ParseNode *parse_relop(Parser *p);
static ParseNode *parse_expr(Parser *p);
static ParseNode *parse_term(Parser *p);
static ParseNode *parse_factor(Parser *p);

/* -- statement* / stmtList (shared by program and block) ------------------- */

static void parse_stmt_sequence(Parser *p, ParseNode *n, TokenType terminator)
{
    TermSet starters = first_statement();

    while (!check(p, terminator) && !check(p, TT_EOF)) {
        if (!TS_HAS(starters, current(p)->type)) {
            char msg[MAX_ERROR_MSG];
            msg_expected(current(p), "start of statement", msg, sizeof msg);
            report(p, msg);

            char label[MAX_LEXEME + 24];
            snprintf(label, sizeof label, "<skipped '%s'>", current(p)->lexeme);
            add(p, n, node(p, label, 1));

            synchronize(p, terminator);
            continue;
        }

        /* Push a landing pad for the duration of this one statement. */
        Landing land;
        land.prev = p->top;
        p->top = &land;

        if (setjmp(land.jb) == 0) {
            ParseNode *stmt = parse_statement(p);
            p->top = land.prev;
            add(p, n, stmt);
        } else {
            /* longjmp landed here: report, mark the tree, and resynchronize */
            p->top = land.prev;

            report(p, p->pending_error);

            char label[MAX_ERROR_MSG + 16];
            snprintf(label, sizeof label, "<error: %s>", p->pending_error);
            add(p, n, node(p, label, 1));

            synchronize(p, terminator);
        }
    }
}

/* -- statement -> declStmt | assignStmt | ifStmt | whileStmt
 *              | block | printStmt */
static ParseNode *parse_statement(Parser *p)
{
    switch (current(p)->type) {
    case TT_INT:    return parse_decl_stmt(p);
    case TT_ID:     return parse_assign_stmt(p);
    case TT_IF:     return parse_if_stmt(p);
    case TT_WHILE:  return parse_while_stmt(p);
    case TT_LBRACE: return parse_block(p);
    case TT_PRINT:  return parse_print_stmt(p);
    default:        fail(p, "start of statement"); return NULL; /* not reached */
    }
}

/* -- declStmt -> "int" ID ";" */
static ParseNode *parse_decl_stmt(Parser *p)
{
    ParseNode *n = node(p, "declStmt", 0);
    add(p, n, leaf(p, advance(p)));        /* "int" (dispatch already checked) */
    add(p, n, expect(p, TT_ID));
    add(p, n, expect(p, TT_SEMI));
    return n;
}

/* -- assignStmt -> ID "=" expr ";" */
static ParseNode *parse_assign_stmt(Parser *p)
{
    ParseNode *n = node(p, "assignStmt", 0);
    add(p, n, leaf(p, advance(p)));        /* ID */
    add(p, n, expect(p, TT_ASSIGN));
    add(p, n, parse_expr(p));
    add(p, n, expect(p, TT_SEMI));
    return n;
}

/* -- ifStmt -> "if" "(" cond ")" block elsePart */
static ParseNode *parse_if_stmt(Parser *p)
{
    ParseNode *n = node(p, "ifStmt", 0);
    add(p, n, leaf(p, advance(p)));        /* "if" */
    add(p, n, expect(p, TT_LPAREN));
    add(p, n, parse_cond(p));
    add(p, n, expect(p, TT_RPAREN));
    add(p, n, parse_block(p));
    add(p, n, parse_else_part(p));
    return n;
}

/* -- elsePart -> "else" block | EPSILON */
static ParseNode *parse_else_part(Parser *p)
{
    if (check(p, TT_ELSE)) {
        ParseNode *n = node(p, "elsePart", 0);
        add(p, n, leaf(p, advance(p)));
        add(p, n, parse_block(p));
        return n;
    }
    return node(p, "elsePart(\xce\xb5)", 0);   /* elsePart(ε) */
}

/* -- whileStmt -> "while" "(" cond ")" block */
static ParseNode *parse_while_stmt(Parser *p)
{
    ParseNode *n = node(p, "whileStmt", 0);
    add(p, n, leaf(p, advance(p)));        /* "while" */
    add(p, n, expect(p, TT_LPAREN));
    add(p, n, parse_cond(p));
    add(p, n, expect(p, TT_RPAREN));
    add(p, n, parse_block(p));
    return n;
}

/* -- block -> "{" stmtList "}" */
static ParseNode *parse_block(Parser *p)
{
    ParseNode *n = node(p, "block", 0);
    add(p, n, expect(p, TT_LBRACE));
    parse_stmt_sequence(p, n, TT_RBRACE);
    add(p, n, expect(p, TT_RBRACE));
    return n;
}

/* -- printStmt -> "print" "(" expr ")" ";" */
static ParseNode *parse_print_stmt(Parser *p)
{
    ParseNode *n = node(p, "printStmt", 0);
    add(p, n, leaf(p, advance(p)));        /* "print" */
    add(p, n, expect(p, TT_LPAREN));
    add(p, n, parse_expr(p));
    add(p, n, expect(p, TT_RPAREN));
    add(p, n, expect(p, TT_SEMI));
    return n;
}

/* -- cond -> expr relop expr */
static ParseNode *parse_cond(Parser *p)
{
    ParseNode *n = node(p, "cond", 0);
    add(p, n, parse_expr(p));
    add(p, n, parse_relop(p));
    add(p, n, parse_expr(p));
    return n;
}

/* -- relop -> "<" | ">" | "<=" | ">=" | "==" | "!=" */
static ParseNode *parse_relop(Parser *p)
{
    switch (current(p)->type) {
    case TT_LT: case TT_GT: case TT_LE:
    case TT_GE: case TT_EQ: case TT_NE: {
        ParseNode *n = node(p, "relop", 0);
        add(p, n, leaf(p, advance(p)));
        return n;
    }
    default:
        fail(p, "relational operator (< > <= >= == !=)");
        return NULL; /* not reached */
    }
}

/* -- expr -> term exprTail   (exprTail right recursion done iteratively) */
static ParseNode *parse_expr(Parser *p)
{
    ParseNode *n = node(p, "expr", 0);
    add(p, n, parse_term(p));
    while (check(p, TT_PLUS) || check(p, TT_MINUS)) {
        add(p, n, leaf(p, advance(p)));
        add(p, n, parse_term(p));
    }
    return n;
}

/* -- term -> factor termTail   (termTail right recursion done iteratively) */
static ParseNode *parse_term(Parser *p)
{
    ParseNode *n = node(p, "term", 0);
    add(p, n, parse_factor(p));
    while (check(p, TT_STAR) || check(p, TT_SLASH)) {
        add(p, n, leaf(p, advance(p)));
        add(p, n, parse_factor(p));
    }
    return n;
}

/* -- factor -> ID | NUM | "(" expr ")" */
static ParseNode *parse_factor(Parser *p)
{
    if (check(p, TT_ID) || check(p, TT_NUM)) {
        ParseNode *n = node(p, "factor", 0);
        add(p, n, leaf(p, advance(p)));
        return n;
    }
    if (check(p, TT_LPAREN)) {
        ParseNode *n = node(p, "factor", 0);
        add(p, n, leaf(p, advance(p)));
        add(p, n, parse_expr(p));
        add(p, n, expect(p, TT_RPAREN));
        return n;
    }
    fail(p, "identifier, number, or '('");
    return NULL; /* not reached */
}

/* -- entry point ----------------------------------------------------------- */

ParseResult parse_tokens(const Token *tokens, size_t count)
{
    /* The parser reads FIRST(statement) out of the grammar tables, so make
     * sure they exist even if the caller forgot to run grammar_init. */
    static int grammar_ready = 0;
    if (!grammar_ready) {
        char err[512];
        if (grammar_init(err, sizeof err) != 0) {
            fprintf(stderr, "%s\n", err);
            exit(1);
        }
        grammar_ready = 1;
    }

    Parser p;
    memset(&p, 0, sizeof p);
    p.tokens = tokens;
    p.count = count;
    p.arena = arena_new();

    pt_reset_steps();

    ParseNode *root = node(&p, "program", 0);
    parse_stmt_sequence(&p, root, TT_EOF);

    ParseResult r;
    r.tree = root;
    r.arena = p.arena;
    r.error_count = p.error_count;
    r.trace = p.trace;
    r.trace_count = p.trace_count;

    r.errors = (char **)arena_alloc(p.arena, sizeof(char *) * (size_t)(p.error_count + 1));
    for (int i = 0; i < p.error_count; i++) r.errors[i] = p.errors[i];

    return r;
}

void parse_result_free(ParseResult *r)
{
    if (!r) return;
    free(r->trace);
    arena_destroy(r->arena);   /* frees the tree, labels and error strings */
    r->trace = NULL;
    r->arena = NULL;
    r->tree = NULL;
    r->errors = NULL;
    r->error_count = 0;
    r->trace_count = 0;
}
