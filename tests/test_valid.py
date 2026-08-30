import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from tokens import Token, TokenType, tok, make_stream
from parser import parse


def test_decl_assign_print_has_no_errors():
    stream = make_stream(
        tok(TokenType.INT, "int", 1), tok(TokenType.ID, "x", 1), tok(TokenType.SEMI, ";", 1),
        tok(TokenType.ID, "x", 2), tok(TokenType.ASSIGN, "=", 2),
        tok(TokenType.NUM, "1", 2), tok(TokenType.PLUS, "+", 2), tok(TokenType.NUM, "2", 2),
        tok(TokenType.SEMI, ";", 2),
        tok(TokenType.PRINT, "print", 3), tok(TokenType.LPAREN, "(", 3),
        tok(TokenType.ID, "x", 3), tok(TokenType.RPAREN, ")", 3), tok(TokenType.SEMI, ";", 3),
    )
    tree, errors = parse(stream)
    assert errors == []
    assert tree.label == "program"
    assert len(tree.children) == 3  # declStmt, assignStmt, printStmt


def test_if_else_has_no_errors():
    stream = make_stream(
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
    tree, errors = parse(stream)
    assert errors == []
    assert tree.children[0].label == "ifStmt"


def test_while_loop_has_no_errors():
    stream = make_stream(
        tok(TokenType.WHILE, "while", 1), tok(TokenType.LPAREN, "(", 1),
        tok(TokenType.ID, "x", 1), tok(TokenType.NE, "!=", 1), tok(TokenType.NUM, "0", 1),
        tok(TokenType.RPAREN, ")", 1), tok(TokenType.LBRACE, "{", 1),
        tok(TokenType.ID, "x", 2), tok(TokenType.ASSIGN, "=", 2),
        tok(TokenType.ID, "x", 2), tok(TokenType.MINUS, "-", 2), tok(TokenType.NUM, "1", 2),
        tok(TokenType.SEMI, ";", 2), tok(TokenType.RBRACE, "}", 3),
    )
    tree, errors = parse(stream)
    assert errors == []
    assert tree.children[0].label == "whileStmt"


def test_nested_parenthesized_expression_has_no_errors():
    # x = (1 + 2) * (3 - 4);
    stream = make_stream(
        tok(TokenType.ID, "x", 1), tok(TokenType.ASSIGN, "=", 1),
        tok(TokenType.LPAREN, "(", 1), tok(TokenType.NUM, "1", 1), tok(TokenType.PLUS, "+", 1),
        tok(TokenType.NUM, "2", 1), tok(TokenType.RPAREN, ")", 1), tok(TokenType.STAR, "*", 1),
        tok(TokenType.LPAREN, "(", 1), tok(TokenType.NUM, "3", 1), tok(TokenType.MINUS, "-", 1),
        tok(TokenType.NUM, "4", 1), tok(TokenType.RPAREN, ")", 1), tok(TokenType.SEMI, ";", 1),
    )
    tree, errors = parse(stream)
    assert errors == []
