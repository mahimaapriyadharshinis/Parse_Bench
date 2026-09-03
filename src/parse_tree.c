#include "parse_tree.h"

#include <stdlib.h>
#include <string.h>

/* -- arena ---------------------------------------------------------------- */

struct Arena {
    void **blocks;
    size_t count;
    size_t cap;
};

static void arena_track(Arena *a, void *p)
{
    if (a->count == a->cap) {
        size_t ncap = a->cap ? a->cap * 2 : 64;
        void **nb = (void **)realloc(a->blocks, ncap * sizeof(void *));
        if (!nb) { fprintf(stderr, "out of memory\n"); exit(1); }
        a->blocks = nb;
        a->cap = ncap;
    }
    a->blocks[a->count++] = p;
}

Arena *arena_new(void)
{
    Arena *a = (Arena *)calloc(1, sizeof(Arena));
    if (!a) { fprintf(stderr, "out of memory\n"); exit(1); }
    return a;
}

void arena_destroy(Arena *a)
{
    if (!a) return;
    for (size_t i = 0; i < a->count; i++) free(a->blocks[i]);
    free(a->blocks);
    free(a);
}

void *arena_alloc(Arena *a, size_t n)
{
    void *p = calloc(1, n);
    if (!p) { fprintf(stderr, "out of memory\n"); exit(1); }
    arena_track(a, p);
    return p;
}

char *arena_strdup(Arena *a, const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = (char *)arena_alloc(a, n);
    memcpy(p, s, n);
    return p;
}

/* Growing a child array allocates a fresh block and leaves the old one for
 * arena_destroy to reclaim. Child counts here are tiny (six at most, from
 * ifStmt) so the wasted memory is negligible, and in exchange no pointer the
 * caller might still hold is ever invalidated by a realloc. */
static void *arena_grow(Arena *a, void *old, size_t oldsz, size_t newsz)
{
    void *p = arena_alloc(a, newsz);
    if (old && oldsz) memcpy(p, old, oldsz);
    return p;
}

/* -- nodes ---------------------------------------------------------------- */

/* Creation order, used by the step-by-step visualizer to reveal the tree in
 * exactly the order the parser built it. Reset by the parser per run. */
static int g_step_counter = 0;

void pt_reset_steps(void) { g_step_counter = 0; }

ParseNode *pt_node(Arena *a, const char *label, int is_error)
{
    ParseNode *n = (ParseNode *)arena_alloc(a, sizeof(ParseNode));
    n->label = arena_strdup(a, label);
    n->is_error = is_error;
    n->step = g_step_counter++;
    return n;
}

ParseNode *pt_add(Arena *a, ParseNode *parent, ParseNode *child)
{
    if (parent->child_count == parent->child_cap) {
        int ncap = parent->child_cap ? parent->child_cap * 2 : 4;
        parent->children = (ParseNode **)arena_grow(
            a, parent->children,
            (size_t)parent->child_cap * sizeof(ParseNode *),
            (size_t)ncap * sizeof(ParseNode *));
        parent->child_cap = ncap;
    }
    parent->children[parent->child_count++] = child;
    return child;
}

int pt_is_leaf(const ParseNode *n)
{
    return n->child_count == 0;
}

int pt_size(const ParseNode *n)
{
    int total = 1;
    for (int i = 0; i < n->child_count; i++) total += pt_size(n->children[i]);
    return total;
}

/* -- text renderer -------------------------------------------------------- */

static void pt_to_text_indented(const ParseNode *n, FILE *out, int indent)
{
    for (int i = 0; i < indent; i++) fputs("  ", out);
    fputs(n->label, out);
    if (n->is_error) fputs(" [ERROR]", out);
    fputc('\n', out);
    for (int i = 0; i < n->child_count; i++)
        pt_to_text_indented(n->children[i], out, indent + 1);
}

void pt_to_text(const ParseNode *n, FILE *out)
{
    pt_to_text_indented(n, out, 0);
}

/* -- Graphviz DOT renderer ------------------------------------------------ */

/* DOT string literals need " and \ escaped. */
static void dot_escape(const char *s, FILE *f)
{
    for (; *s; s++) {
        if (*s == '"' || *s == '\\') fputc('\\', f);
        fputc(*s, f);
    }
}

static int dot_emit(const ParseNode *n, FILE *f, int *counter)
{
    int id = (*counter)++;
    const char *color = n->is_error ? "red" : "black";
    const char *shape = pt_is_leaf(n) ? "box" : "ellipse";

    fprintf(f, "  n%d [label=\"", id);
    dot_escape(n->label, f);
    fprintf(f, "\", color=%s, fontcolor=%s, shape=%s];\n", color, color, shape);

    for (int i = 0; i < n->child_count; i++) {
        int child_id = dot_emit(n->children[i], f, counter);
        fprintf(f, "  n%d -> n%d;\n", id, child_id);
    }
    return id;
}

int pt_write_dot(const ParseNode *n, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) return -1;

    int counter = 0;
    fputs("// Parse Tree\ndigraph {\n", f);
    dot_emit(n, f, &counter);
    fputs("}\n", f);

    int rc = ferror(f) ? -1 : 0;
    if (fclose(f) != 0) rc = -1;
    return rc;
}
