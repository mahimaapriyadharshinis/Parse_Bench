"""Grammar definition (pure BNF, no left recursion) plus generic FIRST/FOLLOW/
LL(1)-table computation.

The restricted language grammar, in EBNF, is:

    program     -> statement*
    statement   -> declStmt | assignStmt | ifStmt | whileStmt | block | printStmt
    declStmt    -> "int" ID ";"
    assignStmt  -> ID "=" expr ";"
    ifStmt      -> "if" "(" cond ")" block ( "else" block )?
    whileStmt   -> "while" "(" cond ")" block
    block       -> "{" statement* "}"
    printStmt   -> "print" "(" expr ")" ";"
    cond        -> expr relop expr
    relop       -> "<" | ">" | "<=" | ">=" | "==" | "!="
    expr        -> term (("+"|"-") term)*
    term        -> factor (("*"|"/") factor)*
    factor      -> ID | NUM | "(" expr ")"

Below is the same grammar rewritten in *pure* BNF (the `*`/`?` repetition
operators expanded into right-recursive rules with an explicit epsilon
production) so that a textbook FIRST/FOLLOW/LL(1)-table algorithm can be run
over it mechanically, exactly as covered in class.
"""

from tokens import TokenType

EPSILON = "EPSILON"
END = TokenType.EOF  # end-of-input marker used in FOLLOW sets

# A production is a list of symbols: TokenType members are terminals,
# strings are non-terminals, and EPSILON marks an empty production.
GRAMMAR: dict[str, list[list]] = {
    "program":    [["statement", "program"], [EPSILON]],
    "statement":  [["declStmt"], ["assignStmt"], ["ifStmt"], ["whileStmt"],
                   ["block"], ["printStmt"]],
    "declStmt":   [[TokenType.INT, TokenType.ID, TokenType.SEMI]],
    "assignStmt": [[TokenType.ID, TokenType.ASSIGN, "expr", TokenType.SEMI]],
    "ifStmt":     [[TokenType.IF, TokenType.LPAREN, "cond", TokenType.RPAREN,
                     "block", "elsePart"]],
    "elsePart":   [[TokenType.ELSE, "block"], [EPSILON]],
    "whileStmt":  [[TokenType.WHILE, TokenType.LPAREN, "cond", TokenType.RPAREN,
                     "block"]],
    "block":      [[TokenType.LBRACE, "stmtList", TokenType.RBRACE]],
    "stmtList":   [["statement", "stmtList"], [EPSILON]],
    "printStmt":  [[TokenType.PRINT, TokenType.LPAREN, "expr", TokenType.RPAREN,
                     TokenType.SEMI]],
    "cond":       [["expr", "relop", "expr"]],
    "relop":      [[TokenType.LT], [TokenType.GT], [TokenType.LE],
                    [TokenType.GE], [TokenType.EQ], [TokenType.NE]],
    "expr":       [["term", "exprTail"]],
    "exprTail":   [[TokenType.PLUS, "term", "exprTail"],
                    [TokenType.MINUS, "term", "exprTail"], [EPSILON]],
    "term":       [["factor", "termTail"]],
    "termTail":   [[TokenType.STAR, "factor", "termTail"],
                    [TokenType.SLASH, "factor", "termTail"], [EPSILON]],
    "factor":     [[TokenType.ID], [TokenType.NUM],
                    [TokenType.LPAREN, "expr", TokenType.RPAREN]],
}

START_SYMBOL = "program"


def is_terminal(symbol) -> bool:
    return isinstance(symbol, TokenType)


def compute_first_sets(grammar: dict) -> dict:
    """Standard fixed-point FIRST-set algorithm."""
    first = {nt: set() for nt in grammar}

    def first_of_sequence(seq) -> set:
        result = set()
        for symbol in seq:
            if symbol == EPSILON:
                result.add(EPSILON)
                return result
            if is_terminal(symbol):
                result.add(symbol)
                return result
            sym_first = first[symbol]
            result |= (sym_first - {EPSILON})
            if EPSILON not in sym_first:
                return result
        result.add(EPSILON)
        return result

    changed = True
    while changed:
        changed = False
        for nt, productions in grammar.items():
            for production in productions:
                before = len(first[nt])
                first[nt] |= first_of_sequence(production)
                if len(first[nt]) != before:
                    changed = True
    return first


def compute_follow_sets(grammar: dict, first: dict, start_symbol: str) -> dict:
    """Standard fixed-point FOLLOW-set algorithm."""
    follow = {nt: set() for nt in grammar}
    follow[start_symbol].add(END)

    def first_of_sequence(seq) -> set:
        result = set()
        for symbol in seq:
            if symbol == EPSILON:
                result.add(EPSILON)
                return result
            if is_terminal(symbol):
                result.add(symbol)
                return result
            sym_first = first[symbol]
            result |= (sym_first - {EPSILON})
            if EPSILON not in sym_first:
                return result
        result.add(EPSILON)
        return result

    changed = True
    while changed:
        changed = False
        for nt, productions in grammar.items():
            for production in productions:
                for i, symbol in enumerate(production):
                    if is_terminal(symbol) or symbol == EPSILON:
                        continue
                    rest = production[i + 1:]
                    rest_first = first_of_sequence(rest) if rest else {EPSILON}
                    before = len(follow[symbol])
                    follow[symbol] |= (rest_first - {EPSILON})
                    if EPSILON in rest_first:
                        follow[symbol] |= follow[nt]
                    if len(follow[symbol]) != before:
                        changed = True
    return follow


def build_ll1_table(grammar: dict, first: dict, follow: dict) -> dict:
    """Build the LL(1) parsing table: (non-terminal, terminal) -> production.

    Also serves as a correctness check: a second production ever trying to
    overwrite an existing table cell means the grammar is *not* LL(1) (a
    FIRST/FIRST or FIRST/FOLLOW conflict) -- this raises immediately so a
    grammar bug is caught at development time rather than causing silent
    mis-parses.
    """

    def first_of_sequence(seq) -> set:
        result = set()
        for symbol in seq:
            if symbol == EPSILON:
                result.add(EPSILON)
                return result
            if is_terminal(symbol):
                result.add(symbol)
                return result
            sym_first = first[symbol]
            result |= (sym_first - {EPSILON})
            if EPSILON not in sym_first:
                return result
        result.add(EPSILON)
        return result

    table: dict[tuple[str, TokenType], list] = {}
    for nt, productions in grammar.items():
        for production in productions:
            prod_first = first_of_sequence(production)
            for terminal in (prod_first - {EPSILON}):
                key = (nt, terminal)
                if key in table:
                    raise ValueError(
                        f"Grammar is not LL(1): conflict at {key} between "
                        f"{table[key]} and {production}"
                    )
                table[key] = production
            if EPSILON in prod_first:
                for terminal in follow[nt]:
                    key = (nt, terminal)
                    if key in table:
                        raise ValueError(
                            f"Grammar is not LL(1): conflict at {key} between "
                            f"{table[key]} and {production}"
                        )
                    table[key] = production
    return table


FIRST = compute_first_sets(GRAMMAR)
FOLLOW = compute_follow_sets(GRAMMAR, FIRST, START_SYMBOL)
LL1_TABLE = build_ll1_table(GRAMMAR, FIRST, FOLLOW)  # raises if grammar isn't LL(1)
