/* Parse Bench's frontend: a full-screen terminal UI in plain C.
 *
 * The UI is redrawn as whole frames. Each frame is built as an array of
 * Lines; a Line holds bytes plus the *visible* width of those bytes, so
 * colour escapes and multi-byte box-drawing characters can be mixed freely
 * and columns still line up. Frames are written starting from the home
 * position with each row erased to end-of-line, so redrawing overwrites the
 * previous frame instead of flashing.
 */
#include "tui.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "grammar.h"
#include "parse_tree.h"
#include "parser.h"
#include "samples.h"
#include "term.h"
#include "token.h"

/* -- lines ----------------------------------------------------------------- */

#define LINE_CAP 8192

typedef struct {
    char b[LINE_CAP];
    int  len;   /* bytes used */
    int  w;     /* visible columns used */
} Line;

static void ln_reset(Line *l) { l->len = 0; l->w = 0; l->b[0] = '\0'; }

/* Append bytes that take up no visible space (colour escapes). */
static void ln_style(Line *l, const char *code)
{
    int n = (int)strlen(code);
    if (l->len + n >= LINE_CAP) return;
    memcpy(l->b + l->len, code, (size_t)n);
    l->len += n;
    l->b[l->len] = '\0';
}

/* Append text, stopping after `maxw` visible columns. UTF-8 continuation
 * bytes (0b10xxxxxx) don't advance the column, so multi-byte characters are
 * never split and never miscounted. */
static void ln_text(Line *l, const char *s, int maxw)
{
    int added = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        int is_continuation = ((*p & 0xC0) == 0x80);
        if (!is_continuation && added >= maxw) break;
        if (l->len + 1 >= LINE_CAP) break;
        l->b[l->len++] = (char)*p;
        if (!is_continuation) { added++; l->w++; }
    }
    l->b[l->len] = '\0';
}

static void ln_textf(Line *l, int maxw, const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    ln_text(l, buf, maxw);
}

/* Pad with spaces until the line is `w` columns wide. */
static void ln_pad_to(Line *l, int w)
{
    while (l->w < w && l->len + 1 < LINE_CAP) {
        l->b[l->len++] = ' ';
        l->w++;
    }
    l->b[l->len] = '\0';
}

/* Repeat a (single-column) string `n` times. */
static void ln_repeat(Line *l, const char *s, int n)
{
    for (int i = 0; i < n; i++) ln_text(l, s, 1);
}

/* -- content rows ---------------------------------------------------------- */

#define ROW_TEXT 480

typedef struct {
    char        text[ROW_TEXT];
    const char *color;      /* NULL for the default colour */
    int         highlight;  /* draw in reverse video */
} Row;

typedef struct {
    Row *v;
    int  n;
    int  cap;
} Rows;

static void rows_init(Rows *r) { r->v = NULL; r->n = 0; r->cap = 0; }
static void rows_clear(Rows *r) { r->n = 0; }
static void rows_free(Rows *r) { free(r->v); rows_init(r); }

static void rows_add(Rows *r, const char *color, int highlight, const char *fmt, ...)
{
    if (r->n == r->cap) {
        int ncap = r->cap ? r->cap * 2 : 64;
        Row *nv = (Row *)realloc(r->v, (size_t)ncap * sizeof(Row));
        if (!nv) { fprintf(stderr, "out of memory\n"); exit(1); }
        r->v = nv;
        r->cap = ncap;
    }
    Row *row = &r->v[r->n++];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(row->text, sizeof row->text, fmt, ap);
    va_end(ap);
    row->color = color;
    row->highlight = highlight;
}

/* -- panels ---------------------------------------------------------------- */

typedef struct {
    int         x0, y0;     /* top-left corner on screen */
    int         w, h;       /* size, borders included */
    const char *title;
    Rows       *rows;
    int         scroll;
    int         focused;
} Panel;

#define BORDER C_GREY

/* Box-drawing characters, spelled out as UTF-8 bytes so the source stays
 * plain ASCII and no compiler needs an encoding flag. */
#define BOX_TL "\xe2\x94\x8c"   /* top-left      */
#define BOX_TR "\xe2\x94\x90"   /* top-right     */
#define BOX_BL "\xe2\x94\x94"   /* bottom-left   */
#define BOX_BR "\xe2\x94\x98"   /* bottom-right  */
#define BOX_H  "\xe2\x94\x80"   /* horizontal    */
#define BOX_V  "\xe2\x94\x82"   /* vertical      */

/* Emit exactly `p->w` visible columns of panel `p` for screen row `y`.
 *
 * All widths are absolute screen columns, not offsets within the panel: a
 * Line accumulates a whole screen row left to right, so the right-hand panel
 * starts at a non-zero column and its borders have to be placed against
 * `p->x0 + p->w`, not against `p->w`. */
static void panel_cell(const Panel *p, int y, Line *l)
{
    int local = y - p->y0;
    int right = p->x0 + p->w;                 /* one past the panel's last column */
    const char *bc = p->focused ? C_CYAN : BORDER;

    if (local == 0) {                          /* top border, with the title in it */
        ln_style(l, bc);
        ln_text(l, BOX_TL BOX_H, 2);
        ln_style(l, p->focused ? C_BOLD C_CYAN : C_WHITE);
        ln_textf(l, right - 2 - l->w, " %s ", p->title);
        ln_style(l, bc);
        while (l->w < right - 1) ln_text(l, BOX_H, 1);
        ln_text(l, BOX_TR, 1);
        ln_style(l, C_RESET);
        return;
    }
    if (local == p->h - 1) {                   /* bottom border */
        ln_style(l, bc);
        ln_text(l, BOX_BL, 1);
        while (l->w < right - 1) ln_text(l, BOX_H, 1);
        ln_text(l, BOX_BR, 1);
        ln_style(l, C_RESET);
        return;
    }

    /* content row: │ <content padded to right-2> │ */
    int idx = p->scroll + (local - 1);
    ln_style(l, bc);
    ln_text(l, BOX_V, 1);
    ln_style(l, C_RESET);
    ln_text(l, " ", 1);

    if (idx >= 0 && idx < p->rows->n) {
        const Row *r = &p->rows->v[idx];
        if (r->highlight) ln_style(l, C_REV);
        if (r->color) ln_style(l, r->color);
        ln_text(l, r->text, right - 2 - l->w);
        /* pad inside the style, so a highlighted row's reverse video runs
         * the full width of the panel rather than stopping at the text */
        while (l->w < right - 2) ln_text(l, " ", 1);
        ln_style(l, C_RESET);
    } else {
        while (l->w < right - 2) ln_text(l, " ", 1);
    }

    ln_text(l, " ", 1);
    ln_style(l, bc);
    ln_text(l, BOX_V, 1);
    ln_style(l, C_RESET);
}

static void panel_scroll_into_view(Panel *p, int idx)
{
    int view = p->h - 2;
    if (view < 1) return;
    if (idx < p->scroll) p->scroll = idx;
    if (idx >= p->scroll + view) p->scroll = idx - view + 1;
    if (p->scroll < 0) p->scroll = 0;
}

/* -- application state ------------------------------------------------------ */

typedef enum { TAB_ANALYZE, TAB_WALK, TAB_GRAMMAR, TAB_INPUT, TAB_COUNT } Tab;

static const char *const TAB_NAMES[TAB_COUNT] = {
    "Analyze", "Walkthrough", "Grammar", "Input"
};

typedef struct {
    char        source[256];   /* what is loaded, for the title bar */
    TokenStream ts;
    ParseResult pr;
    int         loaded;

    Tab  tab;
    int  focus;                /* which panel has the keyboard, per tab */
    int  quit;

    /* walkthrough */
    int  step;                 /* index into pr.trace */
    int  playing;

    /* input tab */
    int  sample_sel;
    int  editing;              /* typing a file path */
    char path_buf[256];
    int  path_len;

    char status[512];

    Rows r_tokens, r_tree, r_errors, r_grammar, r_input;
} App;

static void app_free_parse(App *a)
{
    if (a->loaded) {
        parse_result_free(&a->pr);
        ts_free(&a->ts);
        a->loaded = 0;
    }
}

/* Parse `text` and make it the analyzed stream. Returns 0 on success. */
static int app_load_text(App *a, const char *name, const char *text)
{
    TokenStream ts;
    char err[512];
    ts_init(&ts);
    if (ts_parse_text(text, &ts, err, sizeof err) != 0) {
        snprintf(a->status, sizeof a->status, "%s", err);
        ts_free(&ts);
        return -1;
    }

    app_free_parse(a);
    a->ts = ts;
    a->pr = parse_tokens(a->ts.data, a->ts.count);
    a->loaded = 1;
    a->step = a->pr.trace_count > 0 ? a->pr.trace_count - 1 : 0;
    a->playing = 0;
    snprintf(a->source, sizeof a->source, "%s", name);
    snprintf(a->status, sizeof a->status, "Loaded %s: %d token%s, %d error%s",
             name, (int)a->ts.count, a->ts.count == 1 ? "" : "s",
             a->pr.error_count, a->pr.error_count == 1 ? "" : "s");
    return 0;
}

static int app_load_file(App *a, const char *path)
{
    TokenStream ts;
    char err[512];
    ts_init(&ts);
    if (ts_parse_file(path, &ts, err, sizeof err) != 0) {
        snprintf(a->status, sizeof a->status, "%s", err);
        ts_free(&ts);
        return -1;
    }

    app_free_parse(a);
    a->ts = ts;
    a->pr = parse_tokens(a->ts.data, a->ts.count);
    a->loaded = 1;
    a->step = a->pr.trace_count > 0 ? a->pr.trace_count - 1 : 0;
    a->playing = 0;
    snprintf(a->source, sizeof a->source, "%s", path);
    snprintf(a->status, sizeof a->status, "Loaded %s: %d token%s, %d error%s",
             path, (int)a->ts.count, a->ts.count == 1 ? "" : "s",
             a->pr.error_count, a->pr.error_count == 1 ? "" : "s");
    return 0;
}

/* -- content builders ------------------------------------------------------- */

static const char *token_color(TokenType t)
{
    switch (t) {
    case TT_INT: case TT_IF: case TT_ELSE: case TT_WHILE: case TT_PRINT:
        return C_MAGENTA;
    case TT_ID:  return C_CYAN;
    case TT_NUM: return C_YELLOW;
    case TT_ASSIGN: case TT_PLUS: case TT_MINUS: case TT_STAR: case TT_SLASH:
    case TT_LT: case TT_GT: case TT_LE: case TT_GE: case TT_EQ: case TT_NE:
        return C_GREEN;
    case TT_EOF: return C_GREY;
    default:     return C_WHITE;   /* punctuation */
    }
}

/* `cursor` is the token the parser is looking at, or -1 for none. */
static void build_tokens(App *a, int cursor)
{
    rows_clear(&a->r_tokens);
    if (!a->loaded) return;

    for (size_t i = 0; i < a->ts.count; i++) {
        const Token *t = &a->ts.data[i];
        int is_cur = ((int)i == cursor);
        /* kept narrow enough that the line number still fits in the token
         * panel on an 80-column terminal */
        if (t->type == TT_EOF)
            rows_add(&a->r_tokens, token_color(t->type), is_cur,
                     "%3d %-7s %-10s", (int)i, "EOF", "<end>");
        else
            rows_add(&a->r_tokens, token_color(t->type), is_cur,
                     "%3d %-7s %-10s L%d",
                     (int)i, token_type_name(t->type), t->lexeme, t->line);
    }
}

/* Flatten the tree into indented rows with box-drawing connectors. Nodes
 * created after `max_step` are omitted, which is how the walkthrough shows
 * the tree partway through construction: a node's children always have a
 * higher step than the node itself, so the cut is always a valid subtree. */
static void build_tree_rec(Rows *out, const ParseNode *n, const char *prefix,
                           int is_root, int is_last, int max_step, int cur_step,
                           int *cur_row)
{
    char text[ROW_TEXT];
    const char *color = n->is_error ? C_RED : (pt_is_leaf(n) ? C_CYAN : NULL);
    int highlight = (cur_step >= 0 && n->step == cur_step);

    if (is_root) {
        snprintf(text, sizeof text, "%s%s", n->label, n->is_error ? "  [ERROR]" : "");
    } else {
        snprintf(text, sizeof text, "%s%s%s%s", prefix,
                 is_last ? "\xe2\x94\x94\xe2\x94\x80 " : "\xe2\x94\x9c\xe2\x94\x80 ",
                 n->label, n->is_error ? "  [ERROR]" : "");
    }
    if (highlight) *cur_row = out->n;
    rows_add(out, color, highlight, "%s", text);

    /* which children are visible at this step, and which is the last one */
    int last_visible = -1;
    for (int i = 0; i < n->child_count; i++)
        if (max_step < 0 || n->children[i]->step <= max_step) last_visible = i;
    if (last_visible < 0) return;

    char child_prefix[ROW_TEXT];
    if (is_root) {
        child_prefix[0] = '\0';
    } else {
        snprintf(child_prefix, sizeof child_prefix, "%s%s", prefix,
                 is_last ? "   " : "\xe2\x94\x82  ");
    }

    for (int i = 0; i <= last_visible; i++) {
        if (max_step >= 0 && n->children[i]->step > max_step) continue;
        build_tree_rec(out, n->children[i], child_prefix, 0,
                       i == last_visible, max_step, cur_step, cur_row);
    }
}

static void build_tree(App *a, int max_step, int cur_step, int *cur_row)
{
    rows_clear(&a->r_tree);
    *cur_row = -1;
    if (!a->loaded) return;
    if (max_step >= 0 && a->pr.tree->step > max_step) return;
    build_tree_rec(&a->r_tree, a->pr.tree, "", 1, 1, max_step, cur_step, cur_row);
}

static void build_errors(App *a)
{
    rows_clear(&a->r_errors);
    if (!a->loaded) return;

    if (a->pr.error_count == 0) {
        rows_add(&a->r_errors, C_GREEN, 0, "No syntax errors.");
        return;
    }
    for (int i = 0; i < a->pr.error_count; i++)
        rows_add(&a->r_errors, C_RED, 0, "%d. %s", i + 1, a->pr.errors[i]);
}

static void build_grammar(App *a)
{
    char buf[512];
    Rows *r = &a->r_grammar;
    rows_clear(r);

    rows_add(r, C_BOLD C_WHITE, 0, "PRODUCTIONS  (pure BNF, %d rules)", GRAMMAR_COUNT);
    rows_add(r, NULL, 0, "");
    for (int i = 0; i < GRAMMAR_COUNT; i++) {
        production_rhs_str(&GRAMMAR[i], buf, sizeof buf);
        rows_add(r, NULL, 0, "  %-12s -> %s", nonterm_name(GRAMMAR[i].lhs), buf);
    }

    rows_add(r, NULL, 0, "");
    rows_add(r, C_BOLD C_WHITE, 0, "FIRST SETS  (fixed-point, computed not hand-filled)");
    rows_add(r, NULL, 0, "");
    for (int nt = 0; nt < NT_COUNT; nt++) {
        termset_str(g_first[nt], buf, sizeof buf);
        rows_add(r, C_CYAN, 0, "  FIRST(%-11s) = { %s }", nonterm_name((NonTerm)nt), buf);
    }

    rows_add(r, NULL, 0, "");
    rows_add(r, C_BOLD C_WHITE, 0, "FOLLOW SETS");
    rows_add(r, NULL, 0, "");
    for (int nt = 0; nt < NT_COUNT; nt++) {
        termset_str(g_follow[nt], buf, sizeof buf);
        rows_add(r, C_YELLOW, 0, "  FOLLOW(%-10s) = { %s }", nonterm_name((NonTerm)nt), buf);
    }

    rows_add(r, NULL, 0, "");
    rows_add(r, C_BOLD C_WHITE, 0, "LL(1) PARSING TABLE  (%d conflict-free entries)",
             grammar_table_entries());
    rows_add(r, C_GREEN, 0,
             "  Built without one FIRST/FIRST or FIRST/FOLLOW conflict --");
    rows_add(r, C_GREEN, 0,
             "  which is the proof that this grammar is LL(1).");
    rows_add(r, NULL, 0, "");
    for (int nt = 0; nt < NT_COUNT; nt++) {
        for (int t = 0; t < TT_COUNT; t++) {
            int p = g_table[nt][t];
            if (p < 0) continue;
            const char *term = token_default_lexeme((TokenType)t);
            if (!term || !*term) term = token_type_name((TokenType)t);
            production_rhs_str(&GRAMMAR[p], buf, sizeof buf);
            rows_add(r, NULL, 0, "  M[%-11s, %-6s] = %s -> %s",
                     nonterm_name((NonTerm)nt), term, nonterm_name((NonTerm)nt), buf);
        }
    }
}

static void build_input(App *a)
{
    Rows *r = &a->r_input;
    rows_clear(r);

    rows_add(r, C_BOLD C_WHITE, 0, "BUILT-IN SAMPLES     Enter loads the selected one");
    rows_add(r, NULL, 0, "");
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        int sel = (i == a->sample_sel);
        rows_add(r, sel ? C_CYAN : NULL, sel && !a->editing,
                 "  %-10s  %s", SAMPLES[i].name, SAMPLES[i].description);
    }

    rows_add(r, NULL, 0, "");
    rows_add(r, C_BOLD C_WHITE, 0, "OPEN A TOKEN-STREAM FILE     press o, then type a path");
    rows_add(r, NULL, 0, "");
    if (a->editing)
        rows_add(r, C_CYAN, 1, "  path> %s_", a->path_buf);
    else
        rows_add(r, C_GREY, 0, "  path> (press o)");

    rows_add(r, NULL, 0, "");
    rows_add(r, C_BOLD C_WHITE, 0, "TOKEN-STREAM FORMAT");
    rows_add(r, NULL, 0, "");
    rows_add(r, C_GREY, 0, "  One or more tokens per line, whitespace-separated.");
    rows_add(r, C_GREY, 0, "  Each token is TYPE or TYPE(lexeme); '#' starts a comment.");
    rows_add(r, C_GREY, 0, "  The lexeme is required for ID and NUM, optional otherwise.");
    rows_add(r, NULL, 0, "");
    rows_add(r, C_GREEN, 0, "    INT ID(count) SEMI                  # int count;");
    rows_add(r, C_GREEN, 0, "    ID(count) ASSIGN NUM(0) SEMI        # count = 0;");
    rows_add(r, NULL, 0, "");
    rows_add(r, C_GREY, 0, "  Valid types:");
    {
        char buf[512];
        size_t off = 0;
        buf[0] = '\0';
        for (int t = 0; t < TT_COUNT; t++)
            off += (size_t)snprintf(buf + off, sizeof buf - off, "%s%s",
                                    t ? " " : "    ", token_type_name((TokenType)t));
        rows_add(r, C_GREY, 0, "%s", buf);
    }
}

/* -- frame ------------------------------------------------------------------ */

static void draw_titlebar(Line *l, App *a, int cols)
{
    ln_style(l, C_REV C_BOLD);
    ln_textf(l, cols, " Parse Bench ");
    ln_style(l, C_RESET C_REV);
    ln_textf(l, cols - l->w, " %s ", a->loaded ? a->source : "(nothing loaded)");
    ln_pad_to(l, cols);
    ln_style(l, C_RESET);
}

static void draw_tabbar(Line *l, App *a, int cols)
{
    for (int i = 0; i < TAB_COUNT; i++) {
        if (i == (int)a->tab) ln_style(l, C_REV C_BOLD C_CYAN);
        else                  ln_style(l, C_GREY);
        ln_textf(l, cols - l->w, " %d %s ", i + 1, TAB_NAMES[i]);
        ln_style(l, C_RESET);
        ln_text(l, " ", 1);
    }
    ln_pad_to(l, cols);
}

static void draw_statusbar(Line *l, App *a, int cols)
{
    ln_style(l, C_GREY);
    ln_textf(l, cols, " %s", a->status);
    ln_pad_to(l, cols);
    ln_style(l, C_RESET);
}

static void draw_keybar(Line *l, App *a, int cols)
{
    const char *hints;
    switch (a->tab) {
    case TAB_WALK:
        hints = " space play/pause  <- -> step  Home/End  r reset  Tab next tab  q quit";
        break;
    case TAB_INPUT:
        hints = a->editing
            ? " type a path  Enter load  Esc cancel"
            : " up/down select  Enter load  o open file  Tab next tab  q quit";
        break;
    case TAB_GRAMMAR:
        hints = " up/down scroll  PgUp/PgDn  Home/End  Tab next tab  q quit";
        break;
    default:
        hints = " up/down scroll  <- -> switch pane  d export .dot  Tab next tab  q quit";
        break;
    }
    ln_style(l, C_REV);
    ln_textf(l, cols, "%s", hints);
    ln_pad_to(l, cols);
    ln_style(l, C_RESET);
}

/* Progress bar for the walkthrough. */
static void draw_progress(Line *l, App *a, int cols)
{
    int total = a->loaded ? a->pr.trace_count : 0;
    int cur = total ? a->step + 1 : 0;

    char label[64];
    snprintf(label, sizeof label, " step %d/%d %s ", cur, total,
             a->playing ? "[playing]" : "[paused] ");

    ln_style(l, C_WHITE);
    ln_text(l, label, cols);

    int barw = cols - l->w - 2;
    if (barw > 4) {
        int filled = total > 1 ? (barw * cur) / total : barw;
        ln_style(l, C_CYAN);
        ln_repeat(l, "\xe2\x96\x88", filled);            /* █ */
        ln_style(l, C_GREY);
        ln_repeat(l, "\xe2\x96\x91", barw - filled);     /* ░ */
    }
    ln_pad_to(l, cols);
    ln_style(l, C_RESET);
}

static void emit_frame(Line *frame, int rows)
{
    term_home();
    for (int y = 0; y < rows; y++) {
        fputs(frame[y].b, stdout);
        fputs("\x1b[0m\x1b[K", stdout);
        if (y + 1 < rows) fputc('\n', stdout);
    }
    term_clear_to_end();
    fflush(stdout);
}

/* Which of `panels` covers screen row `y`, or NULL. */
static Panel *panel_at(Panel **panels, int n, int y)
{
    for (int i = 0; i < n; i++)
        if (y >= panels[i]->y0 && y < panels[i]->y0 + panels[i]->h) return panels[i];
    return NULL;
}

/* -- main loop -------------------------------------------------------------- */

int tui_run(const char *path)
{
    App a;
    memset(&a, 0, sizeof a);
    rows_init(&a.r_tokens);
    rows_init(&a.r_tree);
    rows_init(&a.r_errors);
    rows_init(&a.r_grammar);
    rows_init(&a.r_input);

    if (path) {
        if (app_load_file(&a, path) != 0) {
            fprintf(stderr, "Error reading '%s': %s\n", path, a.status);
            return 1;
        }
    } else {
        app_load_text(&a, SAMPLES[0].name, SAMPLES[0].text);
    }

    build_grammar(&a);

    term_begin();

    Line *frame = NULL;
    int frame_cap = 0;

    /* persistent scroll positions */
    int sc_tokens = 0, sc_tree = 0, sc_errors = 0, sc_grammar = 0, sc_input = 0;

    while (!a.quit) {
        int rows, cols;
        term_size(&rows, &cols);
        if (rows > frame_cap) {
            frame = (Line *)realloc(frame, (size_t)rows * sizeof(Line));
            if (!frame) { term_end(); fprintf(stderr, "out of memory\n"); return 1; }
            frame_cap = rows;
        }
        for (int y = 0; y < rows; y++) ln_reset(&frame[y]);

        /* --- layout ------------------------------------------------------- */
        int content_y = 2;
        int content_h = rows - 4;            /* title, tabs, status, keys */
        if (a.tab == TAB_WALK) content_h -= 1;   /* progress bar */
        if (content_h < 3) content_h = 3;

        /* --- current walkthrough position --------------------------------- */
        int cur_token = -1, max_step = -1, cur_step = -1;
        if (a.tab == TAB_WALK && a.loaded && a.pr.trace_count > 0) {
            if (a.step < 0) a.step = 0;
            if (a.step >= a.pr.trace_count) a.step = a.pr.trace_count - 1;
            cur_token = (int)a.pr.trace[a.step].token_pos;
            max_step  = a.pr.trace[a.step].step;
            cur_step  = max_step;
        }

        /* --- build content ------------------------------------------------ */
        int tree_cur_row = -1;
        build_tokens(&a, cur_token);
        build_tree(&a, max_step, cur_step, &tree_cur_row);
        build_errors(&a);
        build_input(&a);

        /* --- chrome -------------------------------------------------------- */
        draw_titlebar(&frame[0], &a, cols);
        draw_tabbar(&frame[1], &a, cols);
        draw_statusbar(&frame[rows - 2], &a, cols);
        draw_keybar(&frame[rows - 1], &a, cols);

        if (a.tab == TAB_ANALYZE || a.tab == TAB_WALK) {
            int left_w = cols * 2 / 5;
            if (left_w < 30) left_w = 30;
            if (left_w > cols - 26) left_w = cols - 26;
            int right_w = cols - left_w;

            int err_h = 6;
            if (err_h > content_h - 5) err_h = content_h - 5;
            if (err_h < 3) err_h = 3;
            int tok_h = content_h - err_h;

            Panel p_tokens = { .x0 = 0, .y0 = content_y, .w = left_w, .h = tok_h,
                               .title = "Token stream", .rows = &a.r_tokens,
                               .scroll = sc_tokens, .focused = a.focus == 0 };
            Panel p_errors = { .x0 = 0, .y0 = content_y + tok_h, .w = left_w, .h = err_h,
                               .title = "Errors", .rows = &a.r_errors,
                               .scroll = sc_errors, .focused = a.focus == 2 };
            Panel p_tree   = { .x0 = left_w, .y0 = content_y, .w = right_w, .h = content_h,
                               .title = "Parse tree", .rows = &a.r_tree,
                               .scroll = sc_tree, .focused = a.focus == 1 };

            /* the walkthrough drives the panes itself, so keep them in view */
            if (a.tab == TAB_WALK) {
                if (cur_token >= 0) panel_scroll_into_view(&p_tokens, cur_token);
                if (tree_cur_row >= 0) panel_scroll_into_view(&p_tree, tree_cur_row);
                sc_tokens = p_tokens.scroll;
                sc_tree = p_tree.scroll;
            }

            Panel *left[] = { &p_tokens, &p_errors };
            for (int y = 0; y < content_h; y++) {
                Line *l = &frame[content_y + y];
                Panel *lp = panel_at(left, 2, content_y + y);
                if (lp) panel_cell(lp, content_y + y, l);
                else    ln_pad_to(l, left_w);
                panel_cell(&p_tree, content_y + y, l);
            }

            if (a.tab == TAB_WALK)
                draw_progress(&frame[content_y + content_h], &a, cols);

            sc_tokens = p_tokens.scroll;
            sc_errors = p_errors.scroll;
            sc_tree = p_tree.scroll;
        } else if (a.tab == TAB_GRAMMAR) {
            Panel p = { .x0 = 0, .y0 = content_y, .w = cols, .h = content_h,
                        .title = "Grammar, FIRST/FOLLOW, LL(1) table",
                        .rows = &a.r_grammar, .scroll = sc_grammar, .focused = 1 };
            for (int y = 0; y < content_h; y++)
                panel_cell(&p, content_y + y, &frame[content_y + y]);
            sc_grammar = p.scroll;
        } else {
            Panel p = { .x0 = 0, .y0 = content_y, .w = cols, .h = content_h,
                        .title = "Input", .rows = &a.r_input,
                        .scroll = sc_input, .focused = 1 };
            for (int y = 0; y < content_h; y++)
                panel_cell(&p, content_y + y, &frame[content_y + y]);
            sc_input = p.scroll;
        }

        emit_frame(frame, rows);

        /* --- input --------------------------------------------------------- */
        int playing = (a.tab == TAB_WALK && a.playing);
        int key = term_getkey(playing ? 90 : -1);

        if (key == KEY_NONE) {
            /* only reachable while playing: advance one step */
            if (a.loaded && a.step + 1 < a.pr.trace_count) a.step++;
            else a.playing = 0;
            continue;
        }

        /* the path editor swallows most keys */
        if (a.tab == TAB_INPUT && a.editing) {
            if (key == KEY_ESC) {
                a.editing = 0;
                snprintf(a.status, sizeof a.status, "Cancelled.");
            } else if (key == KEY_ENTER) {
                a.editing = 0;
                if (a.path_len > 0) {
                    if (app_load_file(&a, a.path_buf) == 0) a.tab = TAB_ANALYZE;
                }
                a.path_buf[0] = '\0';
                a.path_len = 0;
            } else if (key == KEY_BACKSPACE) {
                if (a.path_len > 0) a.path_buf[--a.path_len] = '\0';
            } else if (key >= 32 && key < 127 && a.path_len < (int)sizeof a.path_buf - 1) {
                a.path_buf[a.path_len++] = (char)key;
                a.path_buf[a.path_len] = '\0';
            }
            continue;
        }

        switch (key) {
        case 'q': case 'Q': case KEY_ESC:
            a.quit = 1;
            break;

        case '\t':
            a.tab = (Tab)(((int)a.tab + 1) % TAB_COUNT);
            a.focus = 0;
            break;
        case '1': a.tab = TAB_ANALYZE; a.focus = 0; break;
        case '2': a.tab = TAB_WALK;    a.focus = 0; break;
        case '3': a.tab = TAB_GRAMMAR; break;
        case '4': a.tab = TAB_INPUT;   break;

        case KEY_UP:
        case KEY_DOWN: {
            int d = (key == KEY_DOWN) ? 1 : -1;
            if (a.tab == TAB_INPUT) {
                a.sample_sel += d;
                if (a.sample_sel < 0) a.sample_sel = 0;
                if (a.sample_sel >= SAMPLE_COUNT) a.sample_sel = SAMPLE_COUNT - 1;
            } else if (a.tab == TAB_GRAMMAR) {
                sc_grammar += d;
                if (sc_grammar < 0) sc_grammar = 0;
                if (sc_grammar > a.r_grammar.n - 1) sc_grammar = a.r_grammar.n - 1;
            } else {
                int *sc = a.focus == 0 ? &sc_tokens : (a.focus == 1 ? &sc_tree : &sc_errors);
                Rows *rr = a.focus == 0 ? &a.r_tokens : (a.focus == 1 ? &a.r_tree : &a.r_errors);
                *sc += d;
                if (*sc < 0) *sc = 0;
                if (*sc > rr->n - 1) *sc = rr->n > 0 ? rr->n - 1 : 0;
            }
            break;
        }

        case KEY_PGUP:
        case KEY_PGDN: {
            int d = (key == KEY_PGDN) ? 10 : -10;
            if (a.tab == TAB_GRAMMAR) {
                sc_grammar += d;
                if (sc_grammar < 0) sc_grammar = 0;
                if (sc_grammar > a.r_grammar.n - 1) sc_grammar = a.r_grammar.n - 1;
            } else if (a.tab == TAB_WALK) {
                a.step += d;
            } else {
                int *sc = a.focus == 0 ? &sc_tokens : (a.focus == 1 ? &sc_tree : &sc_errors);
                *sc += d;
                if (*sc < 0) *sc = 0;
            }
            break;
        }

        case KEY_LEFT:
            if (a.tab == TAB_WALK) { a.playing = 0; if (a.step > 0) a.step--; }
            else if (a.focus > 0) a.focus--;
            break;
        case KEY_RIGHT:
            if (a.tab == TAB_WALK) {
                a.playing = 0;
                if (a.loaded && a.step + 1 < a.pr.trace_count) a.step++;
            } else if (a.focus < 2) a.focus++;
            break;

        case ' ':
            if (a.tab == TAB_WALK) {
                a.playing = !a.playing;
                /* restarting from the end replays from the beginning */
                if (a.playing && a.loaded && a.step + 1 >= a.pr.trace_count) a.step = 0;
            }
            break;

        case KEY_HOME:
            if (a.tab == TAB_WALK) { a.step = 0; a.playing = 0; }
            else if (a.tab == TAB_GRAMMAR) sc_grammar = 0;
            else { sc_tokens = sc_tree = sc_errors = 0; }
            break;
        case KEY_END:
            if (a.tab == TAB_WALK) {
                a.playing = 0;
                if (a.loaded) a.step = a.pr.trace_count > 0 ? a.pr.trace_count - 1 : 0;
            } else if (a.tab == TAB_GRAMMAR) {
                sc_grammar = a.r_grammar.n - 1;
                if (sc_grammar < 0) sc_grammar = 0;
            }
            break;

        case 'r': case 'R':
            if (a.tab == TAB_WALK) { a.step = 0; a.playing = 0; }
            break;

        case 'o': case 'O':
            if (a.tab == TAB_INPUT) {
                a.editing = 1;
                a.path_buf[0] = '\0';
                a.path_len = 0;
            }
            break;

        case KEY_ENTER:
            if (a.tab == TAB_INPUT) {
                if (app_load_text(&a, SAMPLES[a.sample_sel].name,
                                  SAMPLES[a.sample_sel].text) == 0) {
                    a.tab = TAB_ANALYZE;
                    sc_tokens = sc_tree = sc_errors = 0;
                }
            }
            break;

        case 'd': case 'D': {
            if (!a.loaded) break;
            /* keep the filename tame: strip any directory part, and bound the
             * length so the status message below can't be truncated */
            const char *dir_end = a.source;
            for (const char *q = a.source; *q; q++)
                if (*q == '/' || *q == '\\') dir_end = q + 1;
            char base[80];
            snprintf(base, sizeof base, "%s", dir_end);
            char out[96];
            snprintf(out, sizeof out, "parse_tree_%s.dot", base);
            if (pt_write_dot(a.pr.tree, out) == 0)
                snprintf(a.status, sizeof a.status,
                         "Wrote %s -- render with: dot -Tpng %s -o tree.png", out, out);
            else
                snprintf(a.status, sizeof a.status, "Could not write %s", out);
            break;
        }

        default:
            break;
        }

        if (a.tab == TAB_WALK && a.loaded) {
            if (a.step < 0) a.step = 0;
            if (a.pr.trace_count > 0 && a.step >= a.pr.trace_count)
                a.step = a.pr.trace_count - 1;
        }
    }

    term_end();

    free(frame);
    rows_free(&a.r_tokens);
    rows_free(&a.r_tree);
    rows_free(&a.r_errors);
    rows_free(&a.r_grammar);
    rows_free(&a.r_input);
    app_free_parse(&a);
    return 0;
}
