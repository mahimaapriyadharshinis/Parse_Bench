# Parse Bench

A grammar-aware syntax analyzer for a restricted, C-like programming language, given a
token stream: it builds an LL(1) parsing table and a recursive-descent parser directly
from a formal grammar, constructs a parse tree, and detects and recovers from syntax
errors instead of stopping at the first one.

No lexer is included by design — the analyzer's input is a token stream, not source
text. See [Token-stream format](#token-stream-format) below for how to write one.

**Repo:** https://github.com/mahimaapriyadharshinis/Parse_Bench

## Contents

- [Quick start](#quick-start)
- [How it works](#how-it-works)
- [Grammar](#grammar)
- [Project structure](#project-structure)
- [Frontends](#frontends)
- [Running the Python analyzer](#running-the-python-analyzer)
- [Token-stream format](#token-stream-format)
- [Running the tests](#running-the-tests)
- [Cloning and pushing changes](#cloning-and-pushing-changes)

## Quick start

The React app is the primary, fullest way to use this — a step-by-step player that
replays the parser's own execution, a syntax-highlighted parse tree, and the live
FIRST/FOLLOW/LL(1) tables, all computed in your browser:

```
cd frontend/react-app
npm install
npm run dev
```

Then open the URL it prints — use `http://localhost:5173`, not `http://127.0.0.1:5173`
(on some machines Vite's dev server binds to the IPv6 loopback address, and only the
`localhost` hostname resolves to it). It opens straight to the "Step by step" tab —
press Play.

No Node/npm handy? Open [frontend/parse_bench.html](frontend/parse_bench.html) directly
in a browser instead — same features, zero install. See [Frontends](#frontends) for
what each tab does.

Want to run the actual Python the algorithm is written in (the graded source, not a
UI) instead? See [Running the Python analyzer](#running-the-python-analyzer).

## How it works

1. **Grammar** ([grammar.py](grammar.py)) — the language is defined as a pure-BNF
   grammar (left recursion removed, `*`/`?` repetition expanded into right-recursive
   rules with explicit epsilon productions). FIRST sets, FOLLOW sets, and the LL(1)
   parsing table are all computed by generic fixed-point algorithms — nothing is
   hand-filled — and table construction raises immediately if it finds a FIRST/FIRST or
   FIRST/FOLLOW conflict, which is how the grammar is proven LL(1) (75 conflict-free
   table entries).
2. **Parsing** ([parser.py](parser.py)) — a recursive-descent parser with one function
   per non-terminal, and human-readable error messages (e.g. `missing ';' - found
   'print' instead`, not a raw token-type dump). Two error-recovery strategies work
   together:
   - **Phrase-level recovery** for an expected-but-missing token (e.g. a missing `;`):
     the error is reported and parsing continues as if the token were there.
   - **Panic-mode recovery** for a token that can't start any valid construct: the
     error is reported and tokens are skipped until one in FIRST(statement) is found —
     or a `}` that actually matches the block currently being closed. A `}` with
     nothing open to close is itself skipped as garbage rather than treated as a safe
     stopping point; earlier versions treated any `}` as safe, which could loop
     forever on a stray unmatched brace (fixed and covered by a regression test in
     `tests/test_edge_cases.py`).
3. **Parse tree** ([parse_tree.py](parse_tree.py)) — an n-ary tree is built as parsing
   proceeds, with recovered-error nodes flagged. It can be printed as indented text or
   exported as a Graphviz diagram.
4. **Watch it happen** — both frontends (below) include a step-by-step visualizer that
   replays the parser's own execution trace: it highlights the current token, reveals
   parse-tree nodes in the exact order the algorithm built them, and narrates each step
   ("Matched declStmt", "Consumed ';'", a reported error, a panic-mode skip) in a log,
   with Play/Pause/step/scrub controls — the fastest way to actually see the algorithm
   working rather than just its final output.

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
    test_edge_cases.py           includes a regression test for the brace-recovery bug
    test_token_stream_text.py
  frontend/
    parse_bench.html    Zero-install, single-file browser demo (same algorithm, in JS)
    react-app/          The same demo as a proper Vite + React project
      src/lib/            grammar.js / tokenStream.js / parser.js / treeLayout.js /
                           tokenStyle.js — 1:1 ports of the Python core, plus the
                           token-category-to-color mapping shared by every view
      src/hooks/          useTreeZoom.js — auto-fit + zoom for both tree views
      src/components/     Header, Hero (status line), InputPanel, TabsPanel,
                           Walkthrough (step-by-step player), ParseTreeView,
                           TokensView, GrammarView, ErrorsView, TreeZoomControls
```

## Frontends

Both frontends port the exact same algorithm (grammar, FIRST/FOLLOW/LL(1) table,
recursive-descent parser, error recovery) to JavaScript, so everything you see — the
parse tree, the FIRST/FOLLOW sets, the recovered errors, the step-by-step replay — is
computed live from whatever token stream you type in, never precomputed. They have
identical features:

- A **step-by-step walkthrough** ("Step by step" tab, the default view) that replays
  the parser's own trace with Play/Pause/step/scrub controls
- A **parse tree** view, syntax-highlighted by token category (keyword / identifier /
  number — the only decorative color in the UI; everything else is monochrome) with
  zoom in/out/fit-to-view controls, since trees can get wide
- **Tokens**, **Grammar & LL(1)** (the live FIRST/FOLLOW sets and table-entry count),
  and **Errors** tabs

**React app** (`frontend/react-app/`) — the primary way to run this; see
[Quick start](#quick-start) for the commands.

**Standalone demo** (`frontend/parse_bench.html`) — zero install, one file. Double-click
it, or run `start frontend/parse_bench.html` on Windows.

## Running the Python analyzer

This is the actual algorithm source (both frontends are JavaScript ports of it) — run
it directly if you want the CLI, or you're checking the graded implementation itself
rather than the UI. Requires Python 3.9+.

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

21 tests cover valid programs, syntax errors (missing tokens, unmatched braces, garbage
tokens, and multiple independent errors recovered in one pass), edge cases (empty input,
empty blocks, deep nesting, a stray unmatched `}` that must not hang the parser), the
token-stream text format, and a check that the grammar is genuinely LL(1).

## Cloning and pushing changes

```
git clone https://github.com/mahimaapriyadharshinis/Parse_Bench.git
cd Parse_Bench
```

Standard workflow from there — `git add`, `git commit`, `git push origin main`. There's
nothing repo-specific beyond the `.gitignore` already excluding `node_modules/`,
`__pycache__/`, and Graphviz's generated `parse_tree_*` output.
