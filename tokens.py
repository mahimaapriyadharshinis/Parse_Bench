"""Token representation for the syntax analyzer.

The lexer is out of scope for this project (the analyzer is handed a token
stream). This module defines the Token type and a few hand-built token
streams used for manual testing / demos.
"""

import re
from dataclasses import dataclass
from enum import Enum, auto


class TokenType(Enum):
    # keywords
    INT = auto()
    IF = auto()
    ELSE = auto()
    WHILE = auto()
    PRINT = auto()
    # literals / identifiers
    ID = auto()
    NUM = auto()
    # operators
    ASSIGN = auto()      # =
    PLUS = auto()
    MINUS = auto()
    STAR = auto()
    SLASH = auto()
    LT = auto()
    GT = auto()
    LE = auto()
    GE = auto()
    EQ = auto()
    NE = auto()
    # punctuation
    LPAREN = auto()
    RPAREN = auto()
    LBRACE = auto()
    RBRACE = auto()
    SEMI = auto()
    # end of stream
    EOF = auto()


KEYWORDS = {
    "int": TokenType.INT,
    "if": TokenType.IF,
    "else": TokenType.ELSE,
    "while": TokenType.WHILE,
    "print": TokenType.PRINT,
}


@dataclass
class Token:
    type: TokenType
    lexeme: str
    line: int

    def __repr__(self):
        return f"{self.type.name}({self.lexeme!r})@{self.line}"


def tok(ttype: TokenType, lexeme: str, line: int) -> Token:
    return Token(ttype, lexeme, line)


def make_stream(*tokens: Token) -> list[Token]:
    """Append an EOF token and return the stream as a list."""
    return list(tokens) + [Token(TokenType.EOF, "", tokens[-1].line if tokens else 0)]


# ---------------------------------------------------------------------------
# User-defined token streams: a small plain-text format
#
# One or more tokens per line, whitespace-separated, `#` starts a comment.
# Each token is written as TYPE or TYPE(lexeme). TYPE must be one of the
# TokenType names (case-insensitive). The lexeme is optional for token
# types with a fixed spelling (keywords/operators/punctuation) but REQUIRED
# for ID and NUM, since those vary. Each source line becomes one line number
# in error messages. Example:
#
#   INT ID(x) SEMI
#   ID(x) ASSIGN NUM(1) PLUS NUM(2) SEMI   # x = 1 + 2;
#
# See examples/custom.tokens for a full sample file.
# ---------------------------------------------------------------------------

DEFAULT_LEXEME = {
    TokenType.INT: "int", TokenType.IF: "if", TokenType.ELSE: "else",
    TokenType.WHILE: "while", TokenType.PRINT: "print",
    TokenType.ASSIGN: "=", TokenType.PLUS: "+", TokenType.MINUS: "-",
    TokenType.STAR: "*", TokenType.SLASH: "/",
    TokenType.LT: "<", TokenType.GT: ">", TokenType.LE: "<=", TokenType.GE: ">=",
    TokenType.EQ: "==", TokenType.NE: "!=",
    TokenType.LPAREN: "(", TokenType.RPAREN: ")",
    TokenType.LBRACE: "{", TokenType.RBRACE: "}", TokenType.SEMI: ";",
    TokenType.EOF: "",
}

_TOKEN_SPEC_RE = re.compile(r"^([A-Za-z_]+)(?:\((.*)\))?$")


class TokenStreamFormatError(ValueError):
    """Raised when user-supplied token-stream text can't be parsed."""


def _parse_token_spec(piece: str, line_no: int) -> Token:
    match = _TOKEN_SPEC_RE.match(piece)
    if not match:
        raise TokenStreamFormatError(
            f"Line {line_no}: cannot parse token spec '{piece}' "
            f"(expected TYPE or TYPE(lexeme))"
        )
    name, lexeme = match.group(1).upper(), match.group(2)
    try:
        ttype = TokenType[name]
    except KeyError:
        valid = ", ".join(t.name for t in TokenType)
        raise TokenStreamFormatError(
            f"Line {line_no}: unknown token type '{name}' in '{piece}'. "
            f"Valid types: {valid}"
        )
    if lexeme is None:
        lexeme = DEFAULT_LEXEME.get(ttype)
        if lexeme is None:
            raise TokenStreamFormatError(
                f"Line {line_no}: token type {name} needs an explicit lexeme, "
                f"e.g. {name}(x) or {name}(42)"
            )
    return Token(ttype, lexeme, line_no)


def parse_token_stream_text(text: str) -> list[Token]:
    """Parse the plain-text token-stream format described above into a
    Token list (an EOF token is appended automatically if missing)."""
    tokens: list[Token] = []
    for line_no, raw_line in enumerate(text.splitlines(), start=1):
        line = raw_line.split("#", 1)[0]
        for piece in line.split():
            tokens.append(_parse_token_spec(piece, line_no))
    if not tokens or tokens[-1].type != TokenType.EOF:
        last_line = tokens[-1].line if tokens else 1
        tokens.append(Token(TokenType.EOF, "", last_line))
    return tokens


def parse_token_stream_file(path: str) -> list[Token]:
    with open(path, "r", encoding="utf-8") as f:
        return parse_token_stream_text(f.read())


# ---------------------------------------------------------------------------
# Sample token streams for manual demos (see main.py)
# ---------------------------------------------------------------------------

# int x; x = 1 + 2 * 3; print(x);
SAMPLE_VALID = make_stream(
    tok(TokenType.INT, "int", 1), tok(TokenType.ID, "x", 1), tok(TokenType.SEMI, ";", 1),
    tok(TokenType.ID, "x", 2), tok(TokenType.ASSIGN, "=", 2),
    tok(TokenType.NUM, "1", 2), tok(TokenType.PLUS, "+", 2),
    tok(TokenType.NUM, "2", 2), tok(TokenType.STAR, "*", 2), tok(TokenType.NUM, "3", 2),
    tok(TokenType.SEMI, ";", 2),
    tok(TokenType.PRINT, "print", 3), tok(TokenType.LPAREN, "(", 3),
    tok(TokenType.ID, "x", 3), tok(TokenType.RPAREN, ")", 3), tok(TokenType.SEMI, ";", 3),
)

# if (x < 10) { x = x + 1; } else { x = 0; }
SAMPLE_VALID_IF = make_stream(
    tok(TokenType.IF, "if", 1), tok(TokenType.LPAREN, "(", 1),
    tok(TokenType.ID, "x", 1), tok(TokenType.LT, "<", 1), tok(TokenType.NUM, "10", 1),
    tok(TokenType.RPAREN, ")", 1), tok(TokenType.LBRACE, "{", 1),
    tok(TokenType.ID, "x", 2), tok(TokenType.ASSIGN, "=", 2),
    tok(TokenType.ID, "x", 2), tok(TokenType.PLUS, "+", 2), tok(TokenType.NUM, "1", 2),
    tok(TokenType.SEMI, ";", 2), tok(TokenType.RBRACE, "}", 3),
    tok(TokenType.ELSE, "else", 3), tok(TokenType.LBRACE, "{", 3),
    tok(TokenType.ID, "x", 4), tok(TokenType.ASSIGN, "=", 4), tok(TokenType.NUM, "0", 4),
    tok(TokenType.SEMI, ";", 4), tok(TokenType.RBRACE, "}", 5),
)

# int x; x = 1 + 2   <-- missing semicolon, then a second independent error
# print(x;                <-- missing RPAREN
SAMPLE_INVALID = make_stream(
    tok(TokenType.INT, "int", 1), tok(TokenType.ID, "x", 1), tok(TokenType.SEMI, ";", 1),
    tok(TokenType.ID, "x", 2), tok(TokenType.ASSIGN, "=", 2),
    tok(TokenType.NUM, "1", 2), tok(TokenType.PLUS, "+", 2), tok(TokenType.NUM, "2", 2),
    # missing SEMI here
    tok(TokenType.PRINT, "print", 3), tok(TokenType.LPAREN, "(", 3),
    tok(TokenType.ID, "x", 3),
    # missing RPAREN here
    tok(TokenType.SEMI, ";", 3),
)

SAMPLE_EMPTY = make_stream()
