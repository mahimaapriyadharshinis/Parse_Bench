import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from tokens import TokenType, tok, make_stream
from parser import parse


def test_missing_semicolon_is_reported():
    # x = 1 + 2   print(x);   <- missing ';' after the assignment
    stream = make_stream(
        tok(TokenType.ID, "x", 1), tok(TokenType.ASSIGN, "=", 1),
        tok(TokenType.NUM, "1", 1), tok(TokenType.PLUS, "+", 1), tok(TokenType.NUM, "2", 1),
        tok(TokenType.PRINT, "print", 2), tok(TokenType.LPAREN, "(", 2),
        tok(TokenType.ID, "x", 2), tok(TokenType.RPAREN, ")", 2), tok(TokenType.SEMI, ";", 2),
    )
    tree, errors = parse(stream)
    assert len(errors) == 1
    assert "missing ';'" in errors[0]
    # recovery must not drop the rest of the program
    assert tree.children[0].label == "assignStmt"
    assert tree.children[1].label == "printStmt"


def test_missing_closing_paren_is_reported():
    # print(x;
    stream = make_stream(
        tok(TokenType.PRINT, "print", 1), tok(TokenType.LPAREN, "(", 1),
        tok(TokenType.ID, "x", 1), tok(TokenType.SEMI, ";", 1),
    )
    tree, errors = parse(stream)
    assert len(errors) == 1
    assert "missing ')'" in errors[0]


def test_unmatched_opening_brace_is_reported():
    # if (x < 1) { x = 1;
    stream = make_stream(
        tok(TokenType.IF, "if", 1), tok(TokenType.LPAREN, "(", 1),
        tok(TokenType.ID, "x", 1), tok(TokenType.LT, "<", 1), tok(TokenType.NUM, "1", 1),
        tok(TokenType.RPAREN, ")", 1), tok(TokenType.LBRACE, "{", 1),
        tok(TokenType.ID, "x", 2), tok(TokenType.ASSIGN, "=", 2), tok(TokenType.NUM, "1", 2),
        tok(TokenType.SEMI, ";", 2),
    )
    tree, errors = parse(stream)
    assert len(errors) == 1
    assert "missing '}'" in errors[0]


def test_garbage_token_at_statement_start_is_reported_and_skipped():
    # a stray '+' can't start a statement; parsing should skip it and
    # still successfully parse the valid statement that follows
    stream = make_stream(
        tok(TokenType.PLUS, "+", 1),
        tok(TokenType.ID, "x", 2), tok(TokenType.ASSIGN, "=", 2), tok(TokenType.NUM, "1", 2),
        tok(TokenType.SEMI, ";", 2),
    )
    tree, errors = parse(stream)
    assert len(errors) == 1
    assert "start of statement" in errors[0]
    assert tree.children[-1].label == "assignStmt"


def test_multiple_independent_errors_are_all_reported_in_one_pass():
    # int x           <- missing ';'
    # print(x;         <- missing ')'
    # x = 1 + 2;       <- valid, should still parse cleanly after two recoveries
    stream = make_stream(
        tok(TokenType.INT, "int", 1), tok(TokenType.ID, "x", 1),
        tok(TokenType.PRINT, "print", 2), tok(TokenType.LPAREN, "(", 2),
        tok(TokenType.ID, "x", 2), tok(TokenType.SEMI, ";", 2),
        tok(TokenType.ID, "x", 3), tok(TokenType.ASSIGN, "=", 3),
        tok(TokenType.NUM, "1", 3), tok(TokenType.PLUS, "+", 3), tok(TokenType.NUM, "2", 3),
        tok(TokenType.SEMI, ";", 3),
    )
    tree, errors = parse(stream)
    assert len(errors) == 2
    assert tree.children[-1].label == "assignStmt"
