# Parse Bench

A grammar-aware syntax analyzer for a restricted, C-like programming language, given a
token stream: it builds an LL(1) parsing table and a recursive-descent parser directly
from a formal grammar, constructs a parse tree, and detects and recovers from syntax
errors instead of stopping at the first one.

No lexer is included by design — the analyzer's input is a token stream, not source
text. See [Token-stream format](#token-stream-format) below for how to write one.

## Contents

- [How it works](#how-it-works)
- [Grammar](#grammar)
- [Project structure](#project-structure)
- [Running the Python analyzer](#running-the-python-analyzer)
- [Token-stream format](#token-stream-format)
- [Running the tests](#running-the-tests)
- [Frontends](#frontends)

## How it works

1. **Grammar** ([grammar.py](grammar.py)) — the language is defined as a pure-BNF
   grammar (left recursion removed, `*`/`?` repetition expanded into right-recursive
   rules with explicit epsilon productions). FIRST sets, FOLLOW sets, and the LL(1)
   parsing table are all computed by generic fixed-point algorithms — nothing is
   hand-filled — and table construction raises immediately if it finds a FIRST/FIRST or
   FIRST/FOLLOW conflict, which is how the grammar is proven LL(1) (75 conflict-free
   table entries).
2. **Parsing** ([parser.py](parser.py)) — a recursive-descent parser with one function
   per non-terminal. Two error-recovery strategies work together:
   - **Phrase-level recovery** for an expected-but-missing token (e.g. a missing `;`):
     the error is reported and parsing continues as if the token were there.
   - **Panic-mode recovery** for a token that can't start any valid construct: the
     error is reported and tokens are skipped until one in the computed
     `FOLLOW(statement)` set is found, then parsing resumes.
3. **Parse tree** ([parse_tree.py](parse_tree.py)) — an n-ary tree is built as parsing
   proceeds, with recovered-error nodes flagged. It can be printed as indented text or
   exported as a Graphviz diagram.

## Grammar

```
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
```

## Project structure

```
compiler/
  tokens.py           Token/TokenType, sample streams, token-stream text-format parser
  grammar.py          Pure-BNF grammar + FIRST/FOLLOW/LL(1) table computation
  parser.py           Recursive-descent parser with error recovery
  parse_tree.py        ParseTreeNode, text printer, Graphviz exporter
  main.py              CLI: run samples, or --file/--stdin for your own token stream
  requirements.txt
  examples/
    custom.tokens       A worked example in the token-stream text format
  tests/
    test_valid.py
    test_invalid.py
    test_edge_cases.py
    test_token_stream_text.py
  frontend/
    parse_bench.html    Zero-install, single-file browser demo (same algorithm, in JS)
    react-app/          The same demo as a proper Vite + React project
```

## Running the Python analyzer

Requires Python 3.9+.

```
pip install -r requirements.txt

# run the built-in sample streams
python main.py

# run one sample by name: valid | valid_if | invalid | empty
python main.py valid

# parse your own token stream
python main.py --file examples/custom.tokens
python main.py --stdin < examples/custom.tokens
```

Each run prints the parse tree as indented text, lists any syntax errors that were
detected and recovered from, and (if the Graphviz package *and* the Graphviz system
binary from [graphviz.org](https://graphviz.org/download/) are both installed) writes a
`parse_tree_<name>.png` diagram.

## Token-stream format

One or more tokens per line, whitespace-separated. Each token is written `TYPE` or
`TYPE(lexeme)` — `TYPE` must match a `TokenType` name (case-insensitive). A lexeme is
required for `ID`/`NUM` since those vary, and optional elsewhere (keywords/operators/
punctuation have sensible defaults). `#` starts a comment; each source line becomes a
line number in error messages.

```
INT ID(x) SEMI                          # int x;
ID(x) ASSIGN NUM(1) PLUS NUM(2) SEMI    # x = 1 + 2;
PRINT LPAREN ID(x) RPAREN SEMI          # print(x);
```

See [examples/custom.tokens](examples/custom.tokens) for a complete example.

## Running the tests

```
pip install -r requirements.txt
pytest tests/ -v
```

20 tests cover valid programs, syntax errors (missing tokens, unmatched braces, garbage
tokens, and multiple independent errors recovered in one pass), edge cases (empty input,
empty blocks, deep nesting), the token-stream text format, and a check that the grammar
is genuinely LL(1).

## Frontends

Both frontends port the exact same algorithm (grammar, FIRST/FOLLOW/LL(1) table,
recursive-descent parser, error recovery) to JavaScript, so everything you see —
the parse tree, the FIRST/FOLLOW sets, the recovered errors — is computed live from
whatever token stream you type in, not precomputed.

**Standalone demo** — zero install, one file:

```
start frontend/parse_bench.html      # Windows
```

Or just double-click the file. It also runs published as a
[Claude Artifact](https://claude.ai/code/artifact/02eceb5b-024d-474c-9a1a-243a7014119b)
if you'd rather share a link than the file itself.

**React app** — the same demo, built as a proper Vite + React project
(`frontend/react-app/src/lib/` mirrors `grammar.py`/`tokens.py`/`parser.py` 1:1;
`frontend/react-app/src/components/` holds the UI):

```
cd frontend/react-app
npm install
npm run dev
```

Then open the URL it prints. Use `http://localhost:5173`, not `http://127.0.0.1:5173`
— on some machines Vite's dev server binds to the IPv6 loopback address, and only the
`localhost` hostname resolves to it.
