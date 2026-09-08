/* Optional convenience lexer: turns C-like source text into a TokenStream.
 *
 * This sits *in front of* the analyzer, not inside it -- the analyzer still
 * only ever consumes a TokenStream (see token.h), same as when that stream
 * comes from the plain-text token-stream format. This is just a second way
 * to produce one, for people who'd rather write real source than hand-write
 * "INT ID(x) SEMI".
 */
#ifndef LEXER_H
#define LEXER_H

#include "token.h"

/* Tokenize `src` (a C-like program in the grammar described in GRAMMAR.md)
 * into `out`. `out` must already be ts_init'd. Appends a trailing EOF token.
 *
 * Returns 0 on success. On failure returns -1, leaves `out` in a partially
 * filled state, and writes a human-readable reason (including line number)
 * into `err`. */
int lex_source(const char *src, TokenStream *out, char *err, size_t errsz);

#endif /* LEXER_H */
