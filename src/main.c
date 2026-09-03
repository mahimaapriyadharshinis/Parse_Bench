/* Parse Bench -- CLI entry point.
 *
 * Usage:
 *   parsebench                       launch the interactive terminal UI on a sample
 *   parsebench FILE                  launch the terminal UI on a token-stream file
 *   parsebench --cli [NAMES...]      run built-in samples and print the results
 *   parsebench --cli --file FILE     run a token-stream file and print the results
 *   parsebench --cli --stdin         read a token stream from stdin
 *   parsebench --cli --dot ...       also write parse_tree_<name>.dot
 *   parsebench --grammar             print FIRST/FOLLOW sets and the LL(1) table
 *   parsebench --help
 *
 * See src/token.h for the token-stream text format, or examples/custom.tokens
 * for a full sample file.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "grammar.h"
#include "parse_tree.h"
#include "parser.h"
#include "samples.h"
#include "token.h"
#include "tui.h"

static void usage(FILE *out)
{
    fputs(
        "Parse Bench -- a grammar-aware syntax analyzer for a C-like language.\n"
        "\n"
        "Usage:\n"
        "  parsebench                    interactive terminal UI on a built-in sample\n"
        "  parsebench FILE               interactive terminal UI on a token-stream file\n"
        "  parsebench --cli [NAMES...]   run built-in samples, print the results\n"
        "  parsebench --cli --file FILE  run a token-stream file, print the results\n"
        "  parsebench --cli --stdin      read a token stream from stdin\n"
        "  parsebench --cli --dot ...    also write parse_tree_<name>.dot\n"
        "  parsebench --grammar          print FIRST/FOLLOW sets and the LL(1) table\n"
        "  parsebench --help\n"
        "\n"
        "Built-in samples: ", out);
    for (int i = 0; i < SAMPLE_COUNT; i++)
        fprintf(out, "%s%s", i ? ", " : "", SAMPLES[i].name);
    fputs("\n\nToken-stream format: one or more tokens per line, whitespace-separated,\n"
          "'#' starts a comment. Each token is TYPE or TYPE(lexeme); the lexeme is\n"
          "required for ID and NUM. See examples/custom.tokens.\n", out);
}

/* Read all of stdin into a malloc'd buffer. */
static char *read_all_stdin(void)
{
    size_t cap = 4096, len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) { fprintf(stderr, "out of memory\n"); exit(1); }
    for (;;) {
        if (len + 1 >= cap) {
            cap *= 2;
            char *nb = (char *)realloc(buf, cap);
            if (!nb) { free(buf); fprintf(stderr, "out of memory\n"); exit(1); }
            buf = nb;
        }
        size_t got = fread(buf + len, 1, cap - len - 1, stdin);
        len += got;
        if (got == 0) break;
    }
    buf[len] = '\0';
    return buf;
}

/* -- batch mode ------------------------------------------------------------ */

static int run_batch(const char *name, const TokenStream *ts, int write_dot)
{
    printf("\n=== %s ===\n", name);

    ParseResult r = parse_tokens(ts->data, ts->count);

    printf("-- Parse tree --\n");
    pt_to_text(r.tree, stdout);

    if (r.error_count > 0) {
        printf("\n-- %d syntax error%s detected (recovered) --\n",
               r.error_count, r.error_count == 1 ? "" : "s");
        for (int i = 0; i < r.error_count; i++) printf("  %s\n", r.errors[i]);
    } else {
        printf("\n-- No syntax errors --\n");
    }

    if (write_dot) {
        char path[512];
        /* keep the filename tame: strip any directory part of `name` */
        const char *base = name;
        for (const char *q = name; *q; q++) if (*q == '/' || *q == '\\') base = q + 1;
        snprintf(path, sizeof path, "parse_tree_%s.dot", base);
        if (pt_write_dot(r.tree, path) == 0)
            printf("\nGraphviz export written to %s\n"
                   "  render it with: dot -Tpng %s -o %.*s.png\n",
                   path, path, (int)(strlen(path) - 4), path);
        else
            printf("\n(Graphviz export skipped: could not write %s)\n", path);
    }

    int errs = r.error_count;
    parse_result_free(&r);
    return errs;
}

static int cli_mode(int argc, char **argv)
{
    int write_dot = 0;
    const char *file = NULL;
    int use_stdin = 0;
    const char *names[16];
    int name_count = 0;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--dot") == 0) {
            write_dot = 1;
        } else if (strcmp(argv[i], "--stdin") == 0) {
            use_stdin = 1;
        } else if (strcmp(argv[i], "--file") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "--file needs a path\n"); return 1; }
            file = argv[++i];
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "unknown option '%s'\n", argv[i]);
            return 1;
        } else if (name_count < (int)(sizeof names / sizeof names[0])) {
            names[name_count++] = argv[i];
        }
    }

    char err[512];
    TokenStream ts;

    if (file) {
        ts_init(&ts);
        if (ts_parse_file(file, &ts, err, sizeof err) != 0) {
            fprintf(stderr, "Error reading '%s': %s\n", file, err);
            ts_free(&ts);
            return 1;
        }
        int errs = run_batch(file, &ts, write_dot);
        ts_free(&ts);
        return errs > 0 ? 1 : 0;
    }

    if (use_stdin) {
        char *text = read_all_stdin();
        ts_init(&ts);
        if (ts_parse_text(text, &ts, err, sizeof err) != 0) {
            fprintf(stderr, "Error reading stdin: %s\n", err);
            free(text);
            ts_free(&ts);
            return 1;
        }
        free(text);
        int errs = run_batch("stdin", &ts, write_dot);
        ts_free(&ts);
        return errs > 0 ? 1 : 0;
    }

    /* built-in samples: the named ones, or all of them */
    int rc = 0;
    for (int s = 0; s < SAMPLE_COUNT; s++) {
        int wanted = (name_count == 0);
        for (int i = 0; i < name_count && !wanted; i++)
            if (strcmp(names[i], SAMPLES[s].name) == 0) wanted = 1;
        if (!wanted) continue;

        ts_init(&ts);
        if (ts_parse_text(SAMPLES[s].text, &ts, err, sizeof err) != 0) {
            fprintf(stderr, "internal error in sample '%s': %s\n", SAMPLES[s].name, err);
            ts_free(&ts);
            return 1;
        }
        run_batch(SAMPLES[s].name, &ts, write_dot);
        ts_free(&ts);
    }

    /* warn about names that matched nothing */
    for (int i = 0; i < name_count; i++) {
        int found = 0;
        for (int s = 0; s < SAMPLE_COUNT && !found; s++)
            if (strcmp(names[i], SAMPLES[s].name) == 0) found = 1;
        if (!found) {
            fprintf(stderr, "Unknown sample '%s'. Choices:", names[i]);
            for (int s = 0; s < SAMPLE_COUNT; s++) fprintf(stderr, " %s", SAMPLES[s].name);
            fputc('\n', stderr);
            rc = 1;
        }
    }
    return rc;
}

/* -- grammar dump ---------------------------------------------------------- */

static void print_grammar_report(void)
{
    char buf[512];

    printf("Grammar (pure BNF, %d productions)\n", GRAMMAR_COUNT);
    printf("---------------------------------------------------------------\n");
    for (int i = 0; i < GRAMMAR_COUNT; i++) {
        production_rhs_str(&GRAMMAR[i], buf, sizeof buf);
        printf("  %-12s -> %s\n", nonterm_name(GRAMMAR[i].lhs), buf);
    }

    printf("\nFIRST sets\n");
    printf("---------------------------------------------------------------\n");
    for (int nt = 0; nt < NT_COUNT; nt++) {
        termset_str(g_first[nt], buf, sizeof buf);
        printf("  FIRST(%-11s) = { %s }\n", nonterm_name((NonTerm)nt), buf);
    }

    printf("\nFOLLOW sets\n");
    printf("---------------------------------------------------------------\n");
    for (int nt = 0; nt < NT_COUNT; nt++) {
        termset_str(g_follow[nt], buf, sizeof buf);
        printf("  FOLLOW(%-10s) = { %s }\n", nonterm_name((NonTerm)nt), buf);
    }

    printf("\nLL(1) parsing table: %d conflict-free entries\n", grammar_table_entries());
    printf("---------------------------------------------------------------\n");
    for (int nt = 0; nt < NT_COUNT; nt++) {
        for (int t = 0; t < TT_COUNT; t++) {
            int p = g_table[nt][t];
            if (p < 0) continue;
            const char *term = token_default_lexeme((TokenType)t);
            if (!term || !*term) term = token_type_name((TokenType)t);
            production_rhs_str(&GRAMMAR[p], buf, sizeof buf);
            printf("  M[%-11s, %-6s] = %s -> %s\n",
                   nonterm_name((NonTerm)nt), term, nonterm_name((NonTerm)nt), buf);
        }
    }
    printf("\nThe table was built without a single FIRST/FIRST or FIRST/FOLLOW\n"
           "conflict, which is the proof that this grammar is LL(1).\n");
}

/* -------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
    char err[512];
    if (grammar_init(err, sizeof err) != 0) {
        fprintf(stderr, "%s\n", err);
        return 1;
    }

    if (argc >= 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        usage(stdout);
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1], "--grammar") == 0) {
        print_grammar_report();
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1], "--cli") == 0) {
        return cli_mode(argc - 2, argv + 2);
    }
    if (argc >= 2 && argv[1][0] == '-') {
        fprintf(stderr, "unknown option '%s'\n\n", argv[1]);
        usage(stderr);
        return 1;
    }

    /* default: the interactive terminal UI */
    return tui_run(argc >= 2 ? argv[1] : NULL);
}
