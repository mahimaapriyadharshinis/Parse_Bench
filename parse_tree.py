"""Parse tree data structure plus text and Graphviz renderers."""

from __future__ import annotations


class ParseTreeNode:
    """An n-ary parse-tree node.

    `label` is the grammar non-terminal name for interior nodes (e.g.
    "assignStmt") or the literal token text for leaves (e.g. "x", "+").
    `is_error` marks a node inserted to represent a recovered syntax error,
    so it can be highlighted differently when rendered.
    """

    def __init__(self, label: str, children: list["ParseTreeNode"] | None = None,
                 is_error: bool = False):
        self.label = label
        self.children: list[ParseTreeNode] = children or []
        self.is_error = is_error

    def add(self, child: "ParseTreeNode") -> "ParseTreeNode":
        self.children.append(child)
        return child

    def is_leaf(self) -> bool:
        return not self.children


def to_text(node: ParseTreeNode, indent: int = 0) -> str:
    """Render the tree as an indented outline, one node per line."""
    marker = " [ERROR]" if node.is_error else ""
    lines = [f"{'  ' * indent}{node.label}{marker}"]
    for child in node.children:
        lines.append(to_text(child, indent + 1))
    return "\n".join(lines)


def print_tree(node: ParseTreeNode) -> None:
    print(to_text(node))


def to_graphviz(node: ParseTreeNode, filename: str = "parse_tree", fmt: str = "png"):
    """Render the tree with Graphviz and write `filename.<fmt>` to disk.

    Requires the `graphviz` pip package AND the Graphviz system binary
    (https://graphviz.org/download/) to be installed and on PATH. Returns the
    graphviz.Digraph object; raises ImportError with a clear message if the
    `graphviz` package isn't installed.
    """
    try:
        import graphviz
    except ImportError as exc:  # pragma: no cover - environment dependent
        raise ImportError(
            "The 'graphviz' package is required for graphical export. "
            "Install it with: pip install graphviz "
            "(and install the Graphviz system binary from graphviz.org)"
        ) from exc

    dot = graphviz.Digraph(comment="Parse Tree")
    counter = {"n": 0}

    def add_node(n: ParseTreeNode) -> str:
        node_id = f"n{counter['n']}"
        counter["n"] += 1
        color = "red" if n.is_error else "black"
        shape = "box" if n.is_leaf() else "ellipse"
        dot.node(node_id, n.label, color=color, fontcolor=color, shape=shape)
        for child in n.children:
            child_id = add_node(child)
            dot.edge(node_id, child_id)
        return node_id

    add_node(node)
    dot.render(filename, format=fmt, cleanup=True)
    return dot
