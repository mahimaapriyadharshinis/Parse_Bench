/* Parse tree data structure plus text and Graphviz renderers.
 *
 * Nodes are allocated from an arena rather than individually freed. The
 * parser abandons partially-built subtrees when it hits an unrecoverable
 * error (see parser.c), so there is no single owner to free them from; the
 * arena releases the whole tree, orphans included, in one call.
 */
#ifndef PARSE_TREE_H
#define PARSE_TREE_H

#include <stddef.h>
#include <stdio.h>

/* An arena that remembers every block it hands out and frees them together. */
typedef struct Arena Arena;

Arena *arena_new(void);
void   arena_destroy(Arena *a);
void  *arena_alloc(Arena *a, size_t n);
char  *arena_strdup(Arena *a, const char *s);

/* An n-ary parse-tree node.
 *
 * `label` is the grammar non-terminal name for interior nodes (e.g.
 * "assignStmt") or the literal token text for leaves (e.g. "x", "+").
 * `is_error` marks a node inserted to represent a recovered syntax error, so
 * it can be highlighted differently when rendered. `step` is the node's
 * creation order, which the step-by-step visualizer replays. */
typedef struct ParseNode {
    char              *label;
    struct ParseNode **children;
    int                child_count;
    int                child_cap;
    int                is_error;
    int                step;
} ParseNode;

/* Restart the `step` counter; the parser calls this once per run. */
void       pt_reset_steps(void);
ParseNode *pt_node(Arena *a, const char *label, int is_error);
/* Append `child` to `parent` and return `child`, so calls can be chained. */
ParseNode *pt_add(Arena *a, ParseNode *parent, ParseNode *child);
int        pt_is_leaf(const ParseNode *n);
/* Total nodes in the subtree rooted at `n`. */
int        pt_size(const ParseNode *n);

/* Render the tree as an indented outline, one node per line. */
void pt_to_text(const ParseNode *n, FILE *out);

/* Write the tree as a Graphviz DOT file. Unlike the Python original this
 * needs no Graphviz library to run -- only the `dot` binary, and only if you
 * want to turn the .dot into a picture:
 *
 *     dot -Tpng parse_tree_valid.dot -o parse_tree_valid.png
 *
 * Returns 0 on success, -1 if the file could not be written. */
int pt_write_dot(const ParseNode *n, const char *path);

#endif /* PARSE_TREE_H */
