/* Port of the project's pytest suite: 21 tests over valid programs, syntax
 * errors, edge cases, and the token-stream text format. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "grammar.h"
#include "minitest.h"
#include "parse_tree.h"
#include "parser.h"
#include "token.h"
#include "watchdog.h"

/* -- helpers --------------------------------------------------------------- */

/* Build a token stream from the plain-text format and parse it. */
static ParseResult parse_text(const char *text, TokenStream *ts)
{
    char err[512];
    ts_init(ts);
    if (ts_parse_text(text, ts, err, sizeof err) != 0) {
        fprintf(stderr, "test setup failed: %s\n", err);
        exit(1);
    }
    return parse_tokens(ts->data, ts->count);
}

static const char *child_label(const ParseNode *n, int i)
{
    if (i < 0) i += n->child_count;
    if (i < 0 || i >= n->child_count) return "<no such child>";
    return n->children[i]->label;
}

/* -- valid programs -------------------------------------------------------- */

TEST(test_decl_assign_print_has_no_errors)
{
    TokenStream ts;
    ParseResult r = parse_text(
        "INT ID(x) SEMI\n"
        "ID(x) ASSIGN NUM(1) PLUS NUM(2) SEMI\n"
        "PRINT LPAREN ID(x) RPAREN SEMI\n", &ts);

    CHECK_INT(r.error_count, 0);
    CHECK_STR(r.tree->label, "program");
    CHECK_INT(r.tree->child_count, 3);   /* declStmt, assignStmt, printStmt */

    parse_result_free(&r);
    ts_free(&ts);
}

TEST(test_if_else_has_no_errors)
{
    TokenStream ts;
    ParseResult r = parse_text(
        "IF LPAREN ID(x) LT NUM(10) RPAREN LBRACE\n"
        "ID(x) ASSIGN ID(x) PLUS NUM(1) SEMI\n"
        "RBRACE ELSE LBRACE\n"
        "ID(x) ASSIGN NUM(0) SEMI\n"
        "RBRACE\n", &ts);

    CHECK_INT(r.error_count, 0);
    CHECK_STR(child_label(r.tree, 0), "ifStmt");

    parse_result_free(&r);
    ts_free(&ts);
}

TEST(test_while_loop_has_no_errors)
{
    TokenStream ts;
    ParseResult r = parse_text(
        "WHILE LPAREN ID(x) NE NUM(0) RPAREN LBRACE\n"
        "ID(x) ASSIGN ID(x) MINUS NUM(1) SEMI\n"
        "RBRACE\n", &ts);

    CHECK_INT(r.error_count, 0);
    CHECK_STR(child_label(r.tree, 0), "whileStmt");

    parse_result_free(&r);
    ts_free(&ts);
}

TEST(test_nested_parenthesized_expression_has_no_errors)
{
    /* x = (1 + 2) * (3 - 4); */
    TokenStream ts;
    ParseResult r = parse_text(
        "ID(x) ASSIGN LPAREN NUM(1) PLUS NUM(2) RPAREN STAR "
        "LPAREN NUM(3) MINUS NUM(4) RPAREN SEMI\n", &ts);

    CHECK_INT(r.error_count, 0);

    parse_result_free(&r);
    ts_free(&ts);
}

/* -- syntax errors and recovery -------------------------------------------- */

TEST(test_missing_semicolon_is_reported)
{
    /* x = 1 + 2   print(x);   <- missing ';' after the assignment */
    TokenStream ts;
    ParseResult r = parse_text(
        "ID(x) ASSIGN NUM(1) PLUS NUM(2)\n"
        "PRINT LPAREN ID(x) RPAREN SEMI\n", &ts);

    CHECK_INT(r.error_count, 1);
    if (r.error_count >= 1) CHECK_SUBSTR(r.errors[0], "missing ';'");
    /* recovery must not drop the rest of the program */
    CHECK_STR(child_label(r.tree, 0), "assignStmt");
    CHECK_STR(child_label(r.tree, 1), "printStmt");

    parse_result_free(&r);
    ts_free(&ts);
}

TEST(test_missing_closing_paren_is_reported)
{
    /* print(x; */
    TokenStream ts;
    ParseResult r = parse_text("PRINT LPAREN ID(x) SEMI\n", &ts);

    CHECK_INT(r.error_count, 1);
    if (r.error_count >= 1) CHECK_SUBSTR(r.errors[0], "missing ')'");

    parse_result_free(&r);
    ts_free(&ts);
}

TEST(test_unmatched_opening_brace_is_reported)
{
    /* if (x < 1) { x = 1;    <- '}' never arrives */
    TokenStream ts;
    ParseResult r = parse_text(
        "IF LPAREN ID(x) LT NUM(1) RPAREN LBRACE\n"
        "ID(x) ASSIGN NUM(1) SEMI\n", &ts);

    CHECK_INT(r.error_count, 1);
    if (r.error_count >= 1) CHECK_SUBSTR(r.errors[0], "missing '}'");

    parse_result_free(&r);
    ts_free(&ts);
}

TEST(test_garbage_token_at_statement_start_is_reported_and_skipped)
{
    /* a stray '+' can't start a statement; parsing should skip it and still
     * successfully parse the valid statement that follows */
    TokenStream ts;
    ParseResult r = parse_text(
        "PLUS\n"
        "ID(x) ASSIGN NUM(1) SEMI\n", &ts);

    CHECK_INT(r.error_count, 1);
    if (r.error_count >= 1) CHECK_SUBSTR(r.errors[0], "start of statement");
    CHECK_STR(child_label(r.tree, -1), "assignStmt");

    parse_result_free(&r);
    ts_free(&ts);
}

TEST(test_multiple_independent_errors_are_all_reported_in_one_pass)
{
    /* int x        <- missing ';'
     * print(x;     <- missing ')'
     * x = 1 + 2;   <- valid, should still parse cleanly after two recoveries */
    TokenStream ts;
    ParseResult r = parse_text(
        "INT ID(x)\n"
        "PRINT LPAREN ID(x) SEMI\n"
        "ID(x) ASSIGN NUM(1) PLUS NUM(2) SEMI\n", &ts);

    CHECK_INT(r.error_count, 2);
    CHECK_STR(child_label(r.tree, -1), "assignStmt");

    parse_result_free(&r);
    ts_free(&ts);
}

/* -- edge cases ------------------------------------------------------------ */

TEST(test_empty_token_stream_produces_empty_program_no_errors)
{
    TokenStream ts;
    ParseResult r = parse_text("", &ts);

    CHECK_STR(r.tree->label, "program");
    CHECK_INT(r.tree->child_count, 0);
    CHECK_INT(r.error_count, 0);

    parse_result_free(&r);
    ts_free(&ts);
}

TEST(test_empty_block_is_valid)
{
    /* while (x != 0) { } */
    TokenStream ts;
    ParseResult r = parse_text(
        "WHILE LPAREN ID(x) NE NUM(0) RPAREN LBRACE RBRACE\n", &ts);

    CHECK_INT(r.error_count, 0);
    CHECK_STR(child_label(r.tree->children[0], -1), "block");

    parse_result_free(&r);
    ts_free(&ts);
}

TEST(test_deeply_nested_parenthesized_expression)
{
    /* x = ((((1)))); to depth 20 */
    const int depth = 20;
    char text[1024];
    size_t off = (size_t)snprintf(text, sizeof text, "ID(x) ASSIGN ");
    for (int i = 0; i < depth; i++) off += (size_t)snprintf(text + off, sizeof text - off, "LPAREN ");
    off += (size_t)snprintf(text + off, sizeof text - off, "NUM(1) ");
    for (int i = 0; i < depth; i++) off += (size_t)snprintf(text + off, sizeof text - off, "RPAREN ");
    snprintf(text + off, sizeof text - off, "SEMI\n");

    TokenStream ts;
    ParseResult r = parse_text(text, &ts);

    CHECK_INT(r.error_count, 0);

    parse_result_free(&r);
    ts_free(&ts);
}

TEST(test_deeply_nested_blocks)
{
    /* { { { { x = 1; } } } } to depth 15 */
    const int depth = 15;
    char text[1024];
    size_t off = 0;
    text[0] = '\0';
    for (int i = 0; i < depth; i++) off += (size_t)snprintf(text + off, sizeof text - off, "LBRACE ");
    off += (size_t)snprintf(text + off, sizeof text - off, "ID(x) ASSIGN NUM(1) SEMI ");
    for (int i = 0; i < depth; i++) off += (size_t)snprintf(text + off, sizeof text - off, "RBRACE ");
    snprintf(text + off, sizeof text - off, "\n");

    TokenStream ts;
    ParseResult r = parse_text(text, &ts);

    CHECK_INT(r.error_count, 0);

    parse_result_free(&r);
    ts_free(&ts);
}

TEST(test_stray_closing_brace_at_top_level_does_not_hang)
{
    /* A '}' with nothing open to close must be treated as garbage and
     * skipped, not as a safe recovery point -- otherwise synchronize() keeps
     * landing on the same unconsumed '}' forever (regression: this used to
     * hang indefinitely instead of returning). */
    TokenStream ts;
    char err[512];
    ts_init(&ts);
    ts_parse_text("RBRACE RBRACE\n"
                  "ID(x) ASSIGN NUM(1) SEMI\n", &ts, err, sizeof err);

    watchdog_arm(5);
    ParseResult r = parse_tokens(ts.data, ts.count);
    watchdog_disarm();

    CHECK_INT(r.error_count, 1);
    if (r.error_count >= 1) CHECK_SUBSTR(r.errors[0], "start of statement");
    /* recovery must still pick back up and parse what follows */
    CHECK_STR(child_label(r.tree, -1), "assignStmt");

    parse_result_free(&r);
    ts_free(&ts);
}

TEST(test_grammar_is_confirmed_ll1_by_table_construction)
{
    /* build_ll1_table reports a FIRST/FIRST or FIRST/FOLLOW conflict as an
     * error, so a clean grammar_init() already proves the grammar is LL(1);
     * this test just re-asserts it explicitly. */
    char err[512];
    CHECK_INT(grammar_init(err, sizeof err), 0);
    if (err[0]) MT_FAIL("grammar_init reported: %s", err);
    CHECK(grammar_table_entries() > 0);
}

/* -- token-stream text format ---------------------------------------------- */

TEST(test_parses_simple_stream_with_default_lexemes)
{
    TokenStream ts;
    char err[512];
    ts_init(&ts);
    CHECK_INT(ts_parse_text("INT ID(x) SEMI", &ts, err, sizeof err), 0);

    CHECK_INT(ts.count, 4);
    if (ts.count == 4) {
        CHECK_INT(ts.data[0].type, TT_INT);
        CHECK_INT(ts.data[1].type, TT_ID);
        CHECK_INT(ts.data[2].type, TT_SEMI);
        CHECK_INT(ts.data[3].type, TT_EOF);
        CHECK_STR(ts.data[0].lexeme, "int");   /* default lexeme for INT */
        CHECK_STR(ts.data[1].lexeme, "x");     /* explicit lexeme for ID */
    }
    ts_free(&ts);
}

TEST(test_line_numbers_track_source_lines)
{
    TokenStream ts;
    char err[512];
    ts_init(&ts);
    ts_parse_text("INT ID(x) SEMI\nID(x) ASSIGN NUM(1) SEMI", &ts, err, sizeof err);

    CHECK_INT(ts.data[0].line, 1);
    CHECK_INT(ts.data[3].line, 2);   /* ID(x) on line 2 */

    ts_free(&ts);
}

TEST(test_comments_are_ignored)
{
    TokenStream ts;
    char err[512];
    ts_init(&ts);
    ts_parse_text("INT ID(x) SEMI   # this is a comment", &ts, err, sizeof err);

    CHECK_INT(ts.count, 4);   /* INT, ID, SEMI, EOF */

    ts_free(&ts);
}

TEST(test_missing_required_lexeme_is_an_error)
{
    TokenStream ts;
    char err[512];
    ts_init(&ts);
    CHECK_INT(ts_parse_text("ID SEMI", &ts, err, sizeof err), -1);
    CHECK_SUBSTR(err, "needs an explicit lexeme");
    ts_free(&ts);
}

TEST(test_unknown_token_type_is_an_error)
{
    TokenStream ts;
    char err[512];
    ts_init(&ts);
    CHECK_INT(ts_parse_text("FROBNICATE(x)", &ts, err, sizeof err), -1);
    CHECK_SUBSTR(err, "unknown token type");
    ts_free(&ts);
}

TEST(test_full_custom_stream_parses_cleanly_end_to_end)
{
    TokenStream ts;
    ParseResult r = parse_text(
        "    INT ID(x) SEMI\n"
        "    ID(x) ASSIGN NUM(1) PLUS NUM(2) SEMI\n"
        "    PRINT LPAREN ID(x) RPAREN SEMI\n", &ts);

    CHECK_INT(r.error_count, 0);
    CHECK_INT(r.tree->child_count, 3);
    CHECK_STR(child_label(r.tree, 0), "declStmt");
    CHECK_STR(child_label(r.tree, 1), "assignStmt");
    CHECK_STR(child_label(r.tree, 2), "printStmt");

    parse_result_free(&r);
    ts_free(&ts);
}

/* -------------------------------------------------------------------------- */

int main(void)
{
    char err[512];
    if (grammar_init(err, sizeof err) != 0) {
        fprintf(stderr, "%s\n", err);
        return 1;
    }

    RUN(test_decl_assign_print_has_no_errors);
    RUN(test_if_else_has_no_errors);
    RUN(test_while_loop_has_no_errors);
    RUN(test_nested_parenthesized_expression_has_no_errors);

    RUN(test_missing_semicolon_is_reported);
    RUN(test_missing_closing_paren_is_reported);
    RUN(test_unmatched_opening_brace_is_reported);
    RUN(test_garbage_token_at_statement_start_is_reported_and_skipped);
    RUN(test_multiple_independent_errors_are_all_reported_in_one_pass);

    RUN(test_empty_token_stream_produces_empty_program_no_errors);
    RUN(test_empty_block_is_valid);
    RUN(test_deeply_nested_parenthesized_expression);
    RUN(test_deeply_nested_blocks);
    RUN(test_stray_closing_brace_at_top_level_does_not_hang);
    RUN(test_grammar_is_confirmed_ll1_by_table_construction);

    RUN(test_parses_simple_stream_with_default_lexemes);
    RUN(test_line_numbers_track_source_lines);
    RUN(test_comments_are_ignored);
    RUN(test_missing_required_lexeme_is_an_error);
    RUN(test_unknown_token_type_is_an_error);
    RUN(test_full_custom_stream_parses_cleanly_end_to_end);

    return mt_report();
}
