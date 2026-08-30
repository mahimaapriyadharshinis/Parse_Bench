"""Recursive-descent LL(1) parser with parse-tree construction and
combined panic-mode / phrase-level syntax error recovery.

Each function below corresponds 1:1 to a non-terminal in grammar.GRAMMAR.
Two kinds of error handling are used, matching the two classic recovery
strategies:

* "Soft" expect (`self.expect`) -- used for an expected closing/terminating
  token (e.g. a missing ';' or ')'). We report the error but do NOT raise:
  we pretend the token was there (phrase-level recovery) and keep parsing
  from the current token, since the rest of the input is often still valid.

* "Hard" dispatch failure (`ParseError` raised directly) -- used when the
  current token doesn't match ANY alternative of a non-terminal (e.g. a
  statement that starts with a token that isn't int/id/if/while/{/print).
  There's no sensible partial node to build, so we abandon the current
  statement and let `parse_stmt_sequence` catch the exception and run
  `synchronize()` (panic-mode recovery): skip tokens until one in
  FOLLOW(statement) (computed in grammar.py) is found, then resume.
"""

from __future__ import annotations

from tokens import Token, TokenType
from parse_tree import ParseTreeNode
import grammar

FIRST_STATEMENT = {t for t in grammar.FIRST["statement"] if t != grammar.EPSILON}
RELOPS = {TokenType.LT, TokenType.GT, TokenType.LE, TokenType.GE,
          TokenType.EQ, TokenType.NE}

# Human-readable spellings for token types, used to phrase error messages the
# way a person would say them out loud rather than as raw enum names.
FRIENDLY_TYPE = {
    TokenType.SEMI: "';'", TokenType.RPAREN: "')'", TokenType.LPAREN: "'('",
    TokenType.RBRACE: "'}'", TokenType.LBRACE: "'{'", TokenType.ASSIGN: "'='",
    TokenType.ID: "an identifier", TokenType.NUM: "a number",
    TokenType.INT: "'int'", TokenType.ELSE: "'else'", TokenType.EOF: "the end of the file",
}


def friendly_type(ttype: TokenType) -> str:
    return FRIENDLY_TYPE.get(ttype, ttype.name)


def friendly_token(token: Token) -> str:
    if token.type == TokenType.EOF:
        return "the end of the file"
    return f"'{token.lexeme}'"


class ParseError(Exception):
    """`missing_token` is set when this came from a soft `expect()` mismatch
    (phrasing becomes "missing X"); left as None for a hard dispatch failure,
    where `expected_desc` is already a full human phrase ("start of
    statement", "identifier, number, or '('") rather than a token type."""

    def __init__(self, token: Token, expected_desc: str, missing_token: TokenType | None = None):
        self.token = token
        self.expected_desc = expected_desc
        found = friendly_token(token)
        if missing_token is not None:
            message = f"Line {token.line}: missing {friendly_type(missing_token)} - found {found} instead"
        else:
            message = f"Line {token.line}: expected {expected_desc}, but found {found}"
        super().__init__(message)


class Parser:
    def __init__(self, tokens: list[Token]):
        self.tokens = tokens
        self.pos = 0
        self.errors: list[str] = []

    # -- token stream helpers -----------------------------------------
    def current(self) -> Token:
        return self.tokens[self.pos]

    def check(self, ttype: TokenType) -> bool:
        return self.current().type == ttype

    def advance(self) -> Token:
        token = self.tokens[self.pos]
        if token.type != TokenType.EOF:
            self.pos += 1
        return token

    def leaf(self, token: Token) -> ParseTreeNode:
        return ParseTreeNode(token.lexeme or token.type.name)

    def report(self, err: ParseError) -> None:
        self.errors.append(str(err))

    def expect(self, ttype: TokenType) -> ParseTreeNode:
        """Soft expect: phrase-level recovery on mismatch (see module docstring)."""
        if self.check(ttype):
            return self.leaf(self.advance())
        self.report(ParseError(self.current(), friendly_type(ttype), missing_token=ttype))
        return ParseTreeNode(f"<missing {ttype.name}>", is_error=True)

    def synchronize(self, terminator: TokenType) -> None:
        """Panic-mode recovery: skip tokens until one in FIRST(statement) is
        found, or SEMI (consumed), or a RBRACE that actually matches the
        block this call is inside (i.e. equals `terminator`). A RBRACE that
        *isn't* the active terminator is a stray/unmatched '}' with nothing
        open to close -- it must be skipped like any other garbage token,
        not treated as a safe stopping point, or a top-level stray '}'
        would never be consumed and this loops forever."""
        while not self.check(TokenType.EOF):
            if self.check(TokenType.SEMI):
                self.advance()
                return
            if self.check(TokenType.RBRACE):
                if self.current().type == terminator:
                    return
                self.advance()
                continue
            if self.current().type in FIRST_STATEMENT:
                return
            self.advance()

    # -- entry point -----------------------------------------------------
    def parse(self) -> ParseTreeNode:
        node = ParseTreeNode("program")
        self.parse_stmt_sequence(node, terminator=TokenType.EOF)
        return node

    # -- statement* / stmtList (shared by program and block) -------------
    def parse_stmt_sequence(self, node: ParseTreeNode, terminator: TokenType) -> None:
        while not self.check(terminator) and not self.check(TokenType.EOF):
            if self.current().type not in FIRST_STATEMENT:
                self.report(ParseError(self.current(), "start of statement"))
                node.add(ParseTreeNode(
                    f"<skipped '{self.current().lexeme}'>", is_error=True))
                self.synchronize(terminator)
                continue
            try:
                node.add(self.parse_statement())
            except ParseError as err:
                self.report(err)
                node.add(ParseTreeNode(f"<error: {err}>", is_error=True))
                self.synchronize(terminator)

    # -- statement -> declStmt | assignStmt | ifStmt | whileStmt
    #              | block | printStmt
    def parse_statement(self) -> ParseTreeNode:
        dispatch = {
            TokenType.INT: self.parse_decl_stmt,
            TokenType.ID: self.parse_assign_stmt,
            TokenType.IF: self.parse_if_stmt,
            TokenType.WHILE: self.parse_while_stmt,
            TokenType.LBRACE: self.parse_block,
            TokenType.PRINT: self.parse_print_stmt,
        }
        handler = dispatch.get(self.current().type)
        if handler is None:
            raise ParseError(self.current(), "start of statement")
        return handler()

    # -- declStmt -> "int" ID ";"
    def parse_decl_stmt(self) -> ParseTreeNode:
        node = ParseTreeNode("declStmt")
        node.add(self.leaf(self.advance()))  # "int" (dispatch already checked)
        node.add(self.expect(TokenType.ID))
        node.add(self.expect(TokenType.SEMI))
        return node

    # -- assignStmt -> ID "=" expr ";"
    def parse_assign_stmt(self) -> ParseTreeNode:
        node = ParseTreeNode("assignStmt")
        node.add(self.leaf(self.advance()))  # ID
        node.add(self.expect(TokenType.ASSIGN))
        node.add(self.parse_expr())
        node.add(self.expect(TokenType.SEMI))
        return node

    # -- ifStmt -> "if" "(" cond ")" block elsePart
    def parse_if_stmt(self) -> ParseTreeNode:
        node = ParseTreeNode("ifStmt")
        node.add(self.leaf(self.advance()))  # "if"
        node.add(self.expect(TokenType.LPAREN))
        node.add(self.parse_cond())
        node.add(self.expect(TokenType.RPAREN))
        node.add(self.parse_block())
        node.add(self.parse_else_part())
        return node

    # -- elsePart -> "else" block | EPSILON
    def parse_else_part(self) -> ParseTreeNode:
        if self.check(TokenType.ELSE):
            node = ParseTreeNode("elsePart")
            node.add(self.leaf(self.advance()))
            node.add(self.parse_block())
            return node
        return ParseTreeNode("elsePart(ε)")

    # -- whileStmt -> "while" "(" cond ")" block
    def parse_while_stmt(self) -> ParseTreeNode:
        node = ParseTreeNode("whileStmt")
        node.add(self.leaf(self.advance()))  # "while"
        node.add(self.expect(TokenType.LPAREN))
        node.add(self.parse_cond())
        node.add(self.expect(TokenType.RPAREN))
        node.add(self.parse_block())
        return node

    # -- block -> "{" stmtList "}"
    def parse_block(self) -> ParseTreeNode:
        node = ParseTreeNode("block")
        node.add(self.expect(TokenType.LBRACE))
        self.parse_stmt_sequence(node, terminator=TokenType.RBRACE)
        node.add(self.expect(TokenType.RBRACE))
        return node

    # -- printStmt -> "print" "(" expr ")" ";"
    def parse_print_stmt(self) -> ParseTreeNode:
        node = ParseTreeNode("printStmt")
        node.add(self.leaf(self.advance()))  # "print"
        node.add(self.expect(TokenType.LPAREN))
        node.add(self.parse_expr())
        node.add(self.expect(TokenType.RPAREN))
        node.add(self.expect(TokenType.SEMI))
        return node

    # -- cond -> expr relop expr
    def parse_cond(self) -> ParseTreeNode:
        node = ParseTreeNode("cond")
        node.add(self.parse_expr())
        node.add(self.parse_relop())
        node.add(self.parse_expr())
        return node

    # -- relop -> "<" | ">" | "<=" | ">=" | "==" | "!="
    def parse_relop(self) -> ParseTreeNode:
        if self.current().type in RELOPS:
            return ParseTreeNode("relop", [self.leaf(self.advance())])
        raise ParseError(self.current(), "relational operator (< > <= >= == !=)")

    # -- expr -> term exprTail   (exprTail right recursion done iteratively)
    def parse_expr(self) -> ParseTreeNode:
        node = ParseTreeNode("expr")
        node.add(self.parse_term())
        while self.current().type in (TokenType.PLUS, TokenType.MINUS):
            node.add(self.leaf(self.advance()))
            node.add(self.parse_term())
        return node

    # -- term -> factor termTail   (termTail right recursion done iteratively)
    def parse_term(self) -> ParseTreeNode:
        node = ParseTreeNode("term")
        node.add(self.parse_factor())
        while self.current().type in (TokenType.STAR, TokenType.SLASH):
            node.add(self.leaf(self.advance()))
            node.add(self.parse_factor())
        return node

    # -- factor -> ID | NUM | "(" expr ")"
    def parse_factor(self) -> ParseTreeNode:
        if self.current().type in (TokenType.ID, TokenType.NUM):
            return ParseTreeNode("factor", [self.leaf(self.advance())])
        if self.check(TokenType.LPAREN):
            node = ParseTreeNode("factor")
            node.add(self.leaf(self.advance()))
            node.add(self.parse_expr())
            node.add(self.expect(TokenType.RPAREN))
            return node
        raise ParseError(self.current(), "identifier, number, or '('")


def parse(tokens: list[Token]) -> tuple[ParseTreeNode, list[str]]:
    """Convenience entry point: parse a token stream, return (tree, errors)."""
    parser = Parser(tokens)
    tree = parser.parse()
    return tree, parser.errors
