/* Port of the project's pytest suite: 21 tests over valid programs, syntax
 * errors, edge cases, and the token-stream text format. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "grammar.h"
#include "lexer.h"
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

static const ParseNode *child(const ParseNode *n, int i)
{
    if (i < 0) i += n->child_count;
    return n->children[i];
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

/* -- for/break/continue, logical operators (&&, ||, !) ---------------------- */

TEST(test_for_loop_with_init_and_update_has_no_errors)
{
    TokenStream ts;
    ParseResult r = parse_text(
        "FOR LPAREN ID(i) ASSIGN NUM(0) SEMI ID(i) LT NUM(5) SEMI "
        "ID(i) ASSIGN ID(i) PLUS NUM(1) RPAREN LBRACE\n"
        "PRINT LPAREN ID(i) RPAREN SEMI\n"
        "RBRACE\n", &ts);

    CHECK_INT(r.error_count, 0);
    CHECK_STR(child_label(r.tree, 0), "forStmt");
    CHECK_STR(child_label(child(r.tree, 0), 2), "forInit");
    CHECK_STR(child_label(child(r.tree, 0), 6), "forUpdate");

    parse_result_free(&r);
    ts_free(&ts);
}

TEST(test_for_loop_with_empty_init_and_update_is_valid)
{
    TokenStream ts;
    ParseResult r = parse_text(
        "FOR LPAREN SEMI ID(i) LT NUM(5) SEMI RPAREN LBRACE\n"
        "RBRACE\n", &ts);

    CHECK_INT(r.error_count, 0);
    CHECK_STR(child_label(child(r.tree, 0), 2), "forInit(\xce\xb5)");
    CHECK_STR(child_label(child(r.tree, 0), 6), "forUpdate(\xce\xb5)");

    parse_result_free(&r);
    ts_free(&ts);
}

TEST(test_break_and_continue_inside_nested_loops_have_no_errors)
{
    TokenStream ts;
    ParseResult r = parse_text(
        "WHILE LPAREN ID(x) LT NUM(10) RPAREN LBRACE\n"
        "IF LPAREN ID(x) EQ NUM(5) RPAREN LBRACE\n"
        "CONTINUE SEMI\n"
        "RBRACE\n"
        "BREAK SEMI\n"
        "RBRACE\n", &ts);

    CHECK_INT(r.error_count, 0);

    const ParseNode *while_block = child(child(r.tree, 0), -1);  /* whileStmt's block */
    CHECK_STR(child_label(while_block, 1), "ifStmt");
    CHECK_STR(child_label(while_block, 2), "breakStmt");

    parse_result_free(&r);
    ts_free(&ts);
}

TEST(test_and_condition_has_no_errors_and_correct_shape)
{
    TokenStream ts;
    ParseResult r = parse_text(
        "IF LPAREN ID(x) LT NUM(5) AND ID(y) GT NUM(2) RPAREN LBRACE\n"
        "RBRACE\n", &ts);

    CHECK_INT(r.error_count, 0);
    const ParseNode *cond = child(child(r.tree, 0), 2);  /* ifStmt's expr slot */
    CHECK_STR(cond->label, "andExpr");
    CHECK_INT(cond->child_count, 3);
    CHECK_STR(child_label(cond, 0), "relExpr");
    CHECK_STR(child_label(cond, 1), "&&");
    CHECK_STR(child_label(cond, 2), "relExpr");

    parse_result_free(&r);
    ts_free(&ts);
}

TEST(test_or_binds_looser_than_and_no_grouping_parens_needed)
{
    TokenStream ts;
    ParseResult r = parse_text(
        "IF LPAREN ID(x) LT NUM(1) OR ID(y) LT NUM(2) AND ID(z) LT NUM(3) "
        "RPAREN LBRACE\nRBRACE\n", &ts);

    CHECK_INT(r.error_count, 0);
    const ParseNode *cond = child(child(r.tree, 0), 2);
    CHECK_STR(cond->label, "expr");                 /* top level: "||" was used */
    CHECK_STR(child_label(cond, 0), "relExpr");     /* x < 1, no "&&" beside it */
    CHECK_STR(child_label(cond, 1), "||");
    CHECK_STR(child_label(cond, 2), "andExpr");     /* y < 2 && z < 3 binds tighter */

    parse_result_free(&r);
    ts_free(&ts);
}

TEST(test_bare_not_binds_to_the_operand_not_the_whole_comparison)
{
    /* Unified precedence now matches real C: "!" is a unary operator at the
     * tightest level (same as unary "-"), so "!x < 5" parses as "(!x) < 5",
     * NOT "!(x < 5)". To negate a whole comparison, parenthesize it -- see
     * test_parenthesized_condition_grouping_now_works below. */
    TokenStream ts;
    ParseResult r = parse_text(
        "IF LPAREN NOT ID(x) LT NUM(5) RPAREN LBRACE\nRBRACE\n", &ts);

    CHECK_INT(r.error_count, 0);
    const ParseNode *cond = child(child(r.tree, 0), 2);
    CHECK_STR(cond->label, "relExpr");
    CHECK_STR(child_label(cond, 0), "unary");
    CHECK_STR(child_label(cond, 1), "relop");
    const ParseNode *not_node = child(cond, 0);
    CHECK_STR(child_label(not_node, 0), "!");
    CHECK_STR(child_label(not_node, 1), "factor");

    parse_result_free(&r);
    ts_free(&ts);
}

TEST(test_parenthesized_condition_grouping_now_works)
{
    /* The old grammar kept "cond" separate from "expr", so factor's own
     * "(" expr ")" and a hypothetical "(" cond ")" would have been two
     * productions starting with "(" -- a FIRST/FIRST conflict. Unifying
     * conditions and arithmetic into one precedence chain (expr -> andExpr
     * -> relExpr -> addExpr -> mulExpr -> unary -> factor, same as real C)
     * removes the conflict: there's only one "(", at the factor level, and
     * it already accepts the full chain. So both of these now parse. */
    TokenStream ts1, ts2;

    ParseResult r1 = parse_text(
        "IF LPAREN NOT LPAREN ID(x) LT NUM(3) RPAREN RPAREN LBRACE\nRBRACE\n", &ts1);
    CHECK_INT(r1.error_count, 0);
    const ParseNode *cond1 = child(child(r1.tree, 0), 2);
    CHECK_STR(cond1->label, "unary");           /* "!" now applies to the whole (...) */
    CHECK_STR(child_label(cond1, 0), "!");
    CHECK_STR(child_label(cond1, 1), "factor");  /* "(" relExpr ")" */
    parse_result_free(&r1);
    ts_free(&ts1);

    ParseResult r2 = parse_text(
        "IF LPAREN LPAREN ID(x) LT NUM(1) RPAREN AND LPAREN ID(y) GT NUM(2) RPAREN "
        "RPAREN LBRACE\nRBRACE\n", &ts2);
    CHECK_INT(r2.error_count, 0);
    const ParseNode *cond2 = child(child(r2.tree, 0), 2);
    CHECK_STR(cond2->label, "andExpr");
    CHECK_STR(child_label(cond2, 0), "factor");  /* "(" x < 1 ")" */
    CHECK_STR(child_label(cond2, 1), "&&");
    CHECK_STR(child_label(cond2, 2), "factor");  /* "(" y > 2 ")" */
    parse_result_free(&r2);
    ts_free(&ts2);
}

TEST(test_unary_minus_has_no_errors)
{
    TokenStream ts;
    ParseResult r = parse_text(
        "ID(x) ASSIGN MINUS NUM(5) SEMI\n", &ts);

    CHECK_INT(r.error_count, 0);
    CHECK_STR(child_label(r.tree, 0), "assignStmt");

    parse_result_free(&r);
    ts_free(&ts);
}

/* -- lexer (source text -> TokenStream) ------------------------------------- */

TEST(test_lexer_produces_expected_tokens_for_a_simple_program)
{
    TokenStream ts;
    char err[512];
    ts_init(&ts);
    CHECK_INT(lex_source("int x;\nx = 1 + 2;\nprint(x);\n", &ts, err, sizeof err), 0);

    /* int x ; x = 1 + 2 ; print ( x ) ; EOF */
    CHECK_INT((int)ts.count, 15);
    CHECK_INT(ts.data[0].type, TT_INT);
    CHECK_INT(ts.data[1].type, TT_ID);
    CHECK_STR(ts.data[1].lexeme, "x");
    CHECK_INT(ts.data[2].type, TT_SEMI);
    CHECK_INT(ts.data[5].type, TT_NUM);
    CHECK_STR(ts.data[5].lexeme, "1");
    CHECK_INT(ts.data[ts.count - 1].type, TT_EOF);

    ts_free(&ts);
}

TEST(test_lexer_tracks_source_line_numbers)
{
    TokenStream ts;
    char err[512];
    ts_init(&ts);
    CHECK_INT(lex_source("int x;\n\nx = 1;\n", &ts, err, sizeof err), 0);

    CHECK_INT(ts.data[0].line, 1);   /* int */
    CHECK_INT(ts.data[3].line, 3);   /* x (assignment) */

    ts_free(&ts);
}

TEST(test_lexer_skips_line_and_block_comments)
{
    TokenStream ts;
    char err[512];
    ts_init(&ts);
    CHECK_INT(lex_source(
        "int x; // trailing line comment\n"
        "/* a block\n   comment */ x = 2;\n", &ts, err, sizeof err), 0);

    CHECK_INT((int)ts.count, 8); /* int x ; x = 2 ; EOF */
    CHECK_INT(ts.data[3].line, 3); /* x after the block comment, on line 3 */

    ts_free(&ts);
}

TEST(test_lexer_recognizes_two_character_operators)
{
    TokenStream ts;
    char err[512];
    ts_init(&ts);
    CHECK_INT(lex_source("x <= 1 >= 2 == 3 != 4", &ts, err, sizeof err), 0);

    CHECK_INT(ts.data[1].type, TT_LE);
    CHECK_INT(ts.data[3].type, TT_GE);
    CHECK_INT(ts.data[5].type, TT_EQ);
    CHECK_INT(ts.data[7].type, TT_NE);

    ts_free(&ts);
}

TEST(test_lexer_recognizes_for_break_continue_keywords)
{
    TokenStream ts;
    char err[512];
    ts_init(&ts);
    CHECK_INT(lex_source("for break continue x", &ts, err, sizeof err), 0);

    CHECK_INT(ts.data[0].type, TT_FOR);
    CHECK_INT(ts.data[1].type, TT_BREAK);
    CHECK_INT(ts.data[2].type, TT_CONTINUE);
    CHECK_INT(ts.data[3].type, TT_ID);   /* "x" is not a keyword */

    ts_free(&ts);
}

TEST(test_lexer_recognizes_logical_operators)
{
    TokenStream ts;
    char err[512];
    ts_init(&ts);
    CHECK_INT(lex_source("x && y || !z", &ts, err, sizeof err), 0);

    CHECK_INT(ts.data[1].type, TT_AND);
    CHECK_INT(ts.data[3].type, TT_OR);
    CHECK_INT(ts.data[4].type, TT_NOT);

    ts_free(&ts);
}

TEST(test_lexer_reports_lone_ampersand_and_pipe_as_errors)
{
    TokenStream ts1, ts2;
    char err[512];

    ts_init(&ts1);
    CHECK_INT(lex_source("x & y", &ts1, err, sizeof err), -1);
    CHECK_SUBSTR(err, "'&' must be followed by '&'");
    ts_free(&ts1);

    ts_init(&ts2);
    CHECK_INT(lex_source("x | y", &ts2, err, sizeof err), -1);
    CHECK_SUBSTR(err, "'|' must be followed by '|'");
    ts_free(&ts2);
}

TEST(test_lexer_reports_unknown_character)
{
    TokenStream ts;
    char err[512];
    ts_init(&ts);
    CHECK_INT(lex_source("x = 1 @ 2;", &ts, err, sizeof err), -1);
    CHECK_SUBSTR(err, "unexpected character '@'");
    ts_free(&ts);
}

TEST(test_lexer_reports_unterminated_block_comment)
{
    TokenStream ts;
    char err[512];
    ts_init(&ts);
    CHECK_INT(lex_source("int x; /* never closed", &ts, err, sizeof err), -1);
    CHECK_SUBSTR(err, "unterminated block comment");
    ts_free(&ts);
}

TEST(test_lexer_output_round_trips_through_the_text_format)
{
    TokenStream lexed;
    char err[512];
    ts_init(&lexed);
    CHECK_INT(lex_source(
        "int count;\ncount = 0;\n"
        "while (count < 5) {\n  print(count);\n  count = count + 1;\n}\n",
        &lexed, err, sizeof err), 0);

    FILE *tmp = tmpfile();
    CHECK(tmp != NULL);
    ts_write_text(&lexed, tmp);
    rewind(tmp);

    char buf[2048];
    size_t n = fread(buf, 1, sizeof buf - 1, tmp);
    buf[n] = '\0';
    fclose(tmp);

    TokenStream reparsed;
    ts_init(&reparsed);
    CHECK_INT(ts_parse_text(buf, &reparsed, err, sizeof err), 0);

    CHECK_INT((int)reparsed.count, (int)lexed.count);
    for (size_t i = 0; i < lexed.count && i < reparsed.count; i++) {
        CHECK_INT(reparsed.data[i].type, lexed.data[i].type);
        CHECK_STR(reparsed.data[i].lexeme, lexed.data[i].lexeme);
    }

    ts_free(&lexed);
    ts_free(&reparsed);
}

TEST(test_lexer_output_parses_cleanly_end_to_end)
{
    TokenStream ts;
    char err[512];
    ts_init(&ts);
    CHECK_INT(lex_source(
        "int x;\nx = 1 + 2;\nprint(x);\n", &ts, err, sizeof err), 0);

    ParseResult r = parse_tokens(ts.data, ts.count);
    CHECK_INT(r.error_count, 0);
    CHECK_INT(r.tree->child_count, 3);
    CHECK_STR(child_label(r.tree, 0), "declStmt");
    CHECK_STR(child_label(r.tree, 1), "assignStmt");
    CHECK_STR(child_label(r.tree, 2), "printStmt");

    parse_result_free(&r);
    ts_free(&ts);
}

TEST(test_lexer_output_for_loop_with_break_and_and_parses_cleanly)
{
    TokenStream ts;
    char err[512];
    ts_init(&ts);
    CHECK_INT(lex_source(
        "int i;\n"
        "for (i = 0; i < 10 && i != 7; i = i + 1) {\n"
        "  if (i == 3) { break; }\n"
        "}\n", &ts, err, sizeof err), 0);

    ParseResult r = parse_tokens(ts.data, ts.count);
    CHECK_INT(r.error_count, 0);
    CHECK_STR(child_label(r.tree, 1), "forStmt");

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

    RUN(test_for_loop_with_init_and_update_has_no_errors);
    RUN(test_for_loop_with_empty_init_and_update_is_valid);
    RUN(test_break_and_continue_inside_nested_loops_have_no_errors);
    RUN(test_and_condition_has_no_errors_and_correct_shape);
    RUN(test_or_binds_looser_than_and_no_grouping_parens_needed);
    RUN(test_bare_not_binds_to_the_operand_not_the_whole_comparison);
    RUN(test_parenthesized_condition_grouping_now_works);
    RUN(test_unary_minus_has_no_errors);

    RUN(test_lexer_produces_expected_tokens_for_a_simple_program);
    RUN(test_lexer_tracks_source_line_numbers);
    RUN(test_lexer_skips_line_and_block_comments);
    RUN(test_lexer_recognizes_two_character_operators);
    RUN(test_lexer_recognizes_for_break_continue_keywords);
    RUN(test_lexer_recognizes_logical_operators);
    RUN(test_lexer_reports_lone_ampersand_and_pipe_as_errors);
    RUN(test_lexer_reports_unknown_character);
    RUN(test_lexer_reports_unterminated_block_comment);
    RUN(test_lexer_output_round_trips_through_the_text_format);
    RUN(test_lexer_output_parses_cleanly_end_to_end);
    RUN(test_lexer_output_for_loop_with_break_and_and_parses_cleanly);

    return mt_report();
}
