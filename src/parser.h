/* Recursive-descent LL(1) parser with parse-tree construction and combined
 * panic-mode / phrase-level syntax error recovery.
 */
#ifndef PARSER_H
#define PARSER_H

#include "parse_tree.h"
#include "token.h"

#define MAX_ERROR_MSG 256

/* One entry of the execution trace the step-by-step visualizer replays:
 * "after this many nodes had been built, the parser was looking at this
 * token". Steps are recorded in the order nodes are created, so revealing
 * nodes with `step <= i` reconstructs exactly what the tree looked like
 * partway through the parse. */
typedef struct {
    int    step;        /* matches ParseNode.step */
    size_t token_pos;   /* index into the token stream at that moment */
} TraceStep;

typedef struct {
    ParseNode *tree;
    Arena     *arena;          /* owns the tree and the error strings */

    char     **errors;         /* error_count human-readable messages */
    int        error_count;

    TraceStep *trace;
    int        trace_count;
} ParseResult;

/* Parse a token stream. The stream must end with a TT_EOF token (use
 * ts_append_eof). Never fails: syntax errors are recovered from and reported
 * in `errors`. Free with parse_result_free. */
ParseResult parse_tokens(const Token *tokens, size_t count);
void        parse_result_free(ParseResult *r);

/* Human-readable spelling of a token type, e.g. "';'" or "an identifier". */
const char *friendly_type(TokenType t);

#endif /* PARSER_H */
