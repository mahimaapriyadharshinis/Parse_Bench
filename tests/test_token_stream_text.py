import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import pytest

from tokens import TokenType, parse_token_stream_text, TokenStreamFormatError
from parser import parse


def test_parses_simple_stream_with_default_lexemes():
    tokens = parse_token_stream_text("INT ID(x) SEMI")
    types = [t.type for t in tokens]
    assert types == [TokenType.INT, TokenType.ID, TokenType.SEMI, TokenType.EOF]
    assert tokens[0].lexeme == "int"  # default lexeme for INT
    assert tokens[1].lexeme == "x"    # explicit lexeme for ID


def test_line_numbers_track_source_lines():
    text = "INT ID(x) SEMI\nID(x) ASSIGN NUM(1) SEMI"
    tokens = parse_token_stream_text(text)
    assert tokens[0].line == 1
    assert tokens[3].line == 2  # ID(x) on line 2


def test_comments_are_ignored():
    tokens = parse_token_stream_text("INT ID(x) SEMI   # this is a comment")
    assert len(tokens) == 4  # INT, ID, SEMI, EOF


def test_missing_required_lexeme_raises():
    with pytest.raises(TokenStreamFormatError, match="needs an explicit lexeme"):
        parse_token_stream_text("ID SEMI")


def test_unknown_token_type_raises():
    with pytest.raises(TokenStreamFormatError, match="unknown token type"):
        parse_token_stream_text("FROBNICATE(x)")


def test_full_custom_stream_parses_cleanly_end_to_end():
    text = """
    INT ID(x) SEMI
    ID(x) ASSIGN NUM(1) PLUS NUM(2) SEMI
    PRINT LPAREN ID(x) RPAREN SEMI
    """
    tokens = parse_token_stream_text(text)
    tree, errors = parse(tokens)
    assert errors == []
    assert [c.label for c in tree.children] == ["declStmt", "assignStmt", "printStmt"]
