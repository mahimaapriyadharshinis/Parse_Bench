import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from tokens import TokenType, tok, make_stream
from parser import parse


def test_empty_token_stream_produces_empty_program_no_errors():
    tree, errors = parse(make_stream())
    assert tree.label == "program"
    assert tree.children == []
    assert errors == []


def test_empty_block_is_valid():
    # while (x != 0) { }
    stream = make_stream(
        tok(TokenType.WHILE, "while", 1), tok(TokenType.LPAREN, "(", 1),
        tok(TokenType.ID, "x", 1), tok(TokenType.NE, "!=", 1), tok(TokenType.NUM, "0", 1),
        tok(TokenType.RPAREN, ")", 1), tok(TokenType.LBRACE, "{", 1),
        tok(TokenType.RBRACE, "}", 1),
    )
    tree, errors = parse(stream)
    assert errors == []
    block = tree.children[0].children[-1]
    assert block.label == "block"


def test_deeply_nested_parenthesized_expression():
    # x = ((((1))));
    depth = 20
    body = [tok(TokenType.ID, "x", 1), tok(TokenType.ASSIGN, "=", 1)]
    body += [tok(TokenType.LPAREN, "(", 1) for _ in range(depth)]
    body += [tok(TokenType.NUM, "1", 1)]
    body += [tok(TokenType.RPAREN, ")", 1) for _ in range(depth)]
    body += [tok(TokenType.SEMI, ";", 1)]
    tree, errors = parse(make_stream(*body))
    assert errors == []


def test_deeply_nested_blocks():
    # { { { { x = 1; } } } }
    depth = 15
    body = [tok(TokenType.LBRACE, "{", 1) for _ in range(depth)]
    body += [tok(TokenType.ID, "x", 1), tok(TokenType.ASSIGN, "=", 1),
              tok(TokenType.NUM, "1", 1), tok(TokenType.SEMI, ";", 1)]
    body += [tok(TokenType.RBRACE, "}", 1) for _ in range(depth)]
    tree, errors = parse(make_stream(*body))
    assert errors == []


def test_stray_closing_brace_at_top_level_does_not_hang():
    # A '}' with nothing open to close must be treated as garbage and
    # skipped, not as a safe recovery point -- otherwise synchronize()
    # keeps landing on the same unconsumed '}' forever (regression: this
    # used to hang indefinitely instead of returning).
    import concurrent.futures

    stream = make_stream(
        tok(TokenType.RBRACE, "}", 1), tok(TokenType.RBRACE, "}", 1),
        tok(TokenType.ID, "x", 2), tok(TokenType.ASSIGN, "=", 2),
        tok(TokenType.NUM, "1", 2), tok(TokenType.SEMI, ";", 2),
    )
    with concurrent.futures.ThreadPoolExecutor() as pool:
        future = pool.submit(parse, stream)
        tree, errors = future.result(timeout=5)

    assert len(errors) == 1
    assert "start of statement" in errors[0]
    # recovery must still pick back up and parse what follows
    assert tree.children[-1].label == "assignStmt"


def test_grammar_is_confirmed_ll1_by_table_construction():
    # grammar.py raises at import time if LL1_TABLE construction finds a
    # FIRST/FIRST or FIRST/FOLLOW conflict, so a successful import already
    # proves the grammar is LL(1); this test just re-asserts it explicitly.
    import grammar
    assert len(grammar.LL1_TABLE) > 0
