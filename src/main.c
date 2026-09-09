/* Parse Bench -- CLI entry point.
 *
 * Usage:
 *   parsebench                       launch the interactive terminal UI on a sample
 *   parsebench FILE                  launch the terminal UI on a token-stream file
 *   parsebench --cli [NAMES...]      run built-in samples and print the results
 *   parsebench --cli --file FILE     run a token-stream file and print the results
 *   parsebench --cli --stdin         read a token stream from stdin
 *   parsebench --cli --dot ...       also write parse_tree_<name>.dot
 *   parsebench --lex FILE [-o OUT]   convert C-like source into the token-stream format
 *   parsebench --lex --stdin         ...reading the source from stdin instead
 *   parsebench --repl                paste C-like source and open it in the full UI, in a loop
 *   parsebench --grammar             print FIRST/FOLLOW sets and the LL(1) table
 *   parsebench --help
 *
 * See src/token.h for the token-stream text format, or examples/custom.tokens
 * for a full sample file. --lex and --repl are optional convenience
 * front-ends (src/lexer.c) that produce that format from real source text;
 * the analyzer itself still only ever consumes a token stream.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "grammar.h"
#include "lexer.h"
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
        "  parsebench --lex FILE [-o OUT]  convert C-like source to the token-stream format\n"
        "  parsebench --lex --stdin      ...reading the source from stdin instead\n"
        "  parsebench --repl             paste C-like source, opens the full UI, in a loop\n"
        "  parsebench --grammar          print FIRST/FOLLOW sets and the LL(1) table\n"
        "  parsebench --help\n"
        "\n"
        "Built-in samples: ", out);
    for (int i = 0; i < SAMPLE_COUNT; i++)
        fprintf(out, "%s%s", i ? ", " : "", SAMPLES[i].name);
    fputs("\n\nToken-stream format: one or more tokens per line, whitespace-separated,\n"
          "'#' starts a comment. Each token is TYPE or TYPE(lexeme); the lexeme is\n"
          "required for ID and NUM. See examples/custom.tokens. --lex produces this\n"
          "format from real C-like source, e.g. parsebench --lex prog.c -o prog.tokens.\n", out);
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

/* -- lex mode ---------------------------------------------------------------
 *
 * Converts real C-like source text into the plain-text token-stream format
 * that the analyzer (and --cli --file / the interactive UI) reads. This is
 * the only place in the project that reads source text instead of a token
 * stream; everything downstream of it is unaffected. */

static char *read_all_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "Cannot open file: %s\n", path); return NULL; }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); fprintf(stderr, "Cannot read: %s\n", path); return NULL; }
    long size = ftell(f);
    if (size < 0) { fclose(f); fprintf(stderr, "Cannot read: %s\n", path); return NULL; }
    rewind(f);

    char *buf = (char *)malloc((size_t)size + 1);
    if (!buf) { fclose(f); fprintf(stderr, "out of memory\n"); return NULL; }
    size_t got = fread(buf, 1, (size_t)size, f);
    buf[got] = '\0';
    fclose(f);
    return buf;
}

static int lex_mode(int argc, char **argv)
{
    const char *file = NULL;
    const char *out_path = NULL;
    int use_stdin = 0;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--stdin") == 0) {
            use_stdin = 1;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "-o needs a path\n"); return 1; }
            out_path = argv[++i];
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "unknown option '%s'\n", argv[i]);
            return 1;
        } else if (!file) {
            file = argv[i];
        } else {
            fprintf(stderr, "unexpected argument '%s'\n", argv[i]);
            return 1;
        }
    }

    if (!file && !use_stdin) {
        fprintf(stderr, "--lex needs a source FILE or --stdin\n");
        return 1;
    }
    if (file && use_stdin) {
        fprintf(stderr, "--lex takes a FILE or --stdin, not both\n");
        return 1;
    }

    char *text = use_stdin ? read_all_stdin() : read_all_file(file);
    if (!text) return 1;

    TokenStream ts;
    ts_init(&ts);
    char err[512];
    int rc = lex_source(text, &ts, err, sizeof err);
    free(text);
    if (rc != 0) {
        fprintf(stderr, "Lex error: %s\n", err);
        ts_free(&ts);
        return 1;
    }

    FILE *out = stdout;
    if (out_path) {
        out = fopen(out_path, "wb");
        if (!out) {
            fprintf(stderr, "Cannot open file for writing: %s\n", out_path);
            ts_free(&ts);
            return 1;
        }
    }
    ts_write_text(&ts, out);
    if (out_path) fclose(out);

    ts_free(&ts);
    return 0;
}

/* -- repl mode ---------------------------------------------------------------
 *
 * A loop around --lex + the interactive UI: paste C-like source straight
 * into the terminal, an empty line lexes it and opens the result in the same
 * full-screen, colored token stream / parse tree / errors UI as
 * `parsebench FILE` -- no file to hand-manage between attempts. */

static void buf_append(char **buf, size_t *len, size_t *cap, const char *s, size_t slen)
{
    if (*len + slen + 1 > *cap) {
        size_t ncap = *cap ? *cap * 2 : 4096;
        while (ncap < *len + slen + 1) ncap *= 2;
        char *nb = (char *)realloc(*buf, ncap);
        if (!nb) { fprintf(stderr, "out of memory\n"); exit(1); }
        *buf = nb;
        *cap = ncap;
    }
    memcpy(*buf + *len, s, slen);
    *len += slen;
    (*buf)[*len] = '\0';
}

static int is_blank_line(const char *s)
{
    for (; *s; s++) if (!isspace((unsigned char)*s)) return 0;
    return 1;
}

static void trim_trailing_ws(char *s)
{
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
}

/* Read one pasted block: lines up to (and not including) the blank line that
 * ends it, or up to EOF. Sets `*eof` if input ended without a blank line.
 * Returns NULL (and leaves `*eof` meaningful) if the block was empty --
 * either just blank lines, or "quit"/"exit" alone as the very first line. */
static char *read_repl_block(int *eof, int *quit)
{
    char *buf = NULL;
    size_t len = 0, cap = 0;
    int got_any = 0;
    char line[1024];

    *eof = 0;
    *quit = 0;

    for (;;) {
        if (!fgets(line, sizeof line, stdin)) { *eof = 1; break; }

        if (!got_any) {
            char trimmed[sizeof line];
            snprintf(trimmed, sizeof trimmed, "%s", line);
            trim_trailing_ws(trimmed);
            if (strcmp(trimmed, "quit") == 0 || strcmp(trimmed, "exit") == 0) {
                *quit = 1;
                free(buf);
                return NULL;
            }
        }

        if (is_blank_line(line)) {
            if (got_any) break;    /* blank line after real input: run it */
            continue;              /* leading blank lines: keep waiting */
        }

        buf_append(&buf, &len, &cap, line, strlen(line));
        got_any = 1;
    }

    if (!got_any) { free(buf); return NULL; }
    return buf;
}

/* A scratch file the REPL overwrites on every paste, so each block opens in
 * the same full-screen UI everything else uses (colored token stream, parse
 * tree, errors, walkthrough) instead of a plain-text dump. Removed on exit. */
#define REPL_SCRATCH_PATH ".parsebench_repl.tokens"

static int repl_mode(void)
{
    printf("Parse Bench REPL -- paste C-like source, then an empty line to open it\n"
           "in the full terminal UI (same colored token stream / parse tree / errors\n"
           "panes as `parsebench FILE`). Press q to close it and come back here.\n"
           "Type 'quit' or 'exit' alone (or Ctrl-Z+Enter / Ctrl-D on an empty line)"
           " to leave.\n\n");

    for (;;) {
        printf("code> ");
        fflush(stdout);

        int eof, quit;
        char *src = read_repl_block(&eof, &quit);

        if (src) {
            TokenStream ts;
            ts_init(&ts);
            char err[512];
            if (lex_source(src, &ts, err, sizeof err) != 0) {
                printf("Lex error: %s\n\n", err);
            } else {
                FILE *out = fopen(REPL_SCRATCH_PATH, "wb");
                if (!out) {
                    fprintf(stderr, "Cannot write '%s'\n\n", REPL_SCRATCH_PATH);
                } else {
                    ts_write_text(&ts, out);
                    fclose(out);
                    tui_run(REPL_SCRATCH_PATH);   /* blocks until 'q' / Esc */
                }
            }
            ts_free(&ts);
            free(src);
        }

        if (quit || eof) break;
    }
    remove(REPL_SCRATCH_PATH);
    return 0;
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
    if (argc >= 2 && strcmp(argv[1], "--lex") == 0) {
        return lex_mode(argc - 2, argv + 2);
    }
    if (argc >= 2 && strcmp(argv[1], "--repl") == 0) {
        return repl_mode();
    }
    if (argc >= 2 && argv[1][0] == '-') {
        fprintf(stderr, "unknown option '%s'\n\n", argv[1]);
        usage(stderr);
        return 1;
    }

    /* default: the interactive terminal UI */
    return tui_run(argc >= 2 ? argv[1] : NULL);
}
