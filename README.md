# Parse Bench

A grammar-aware syntax analyzer for a restricted, C-like programming language,
given a token stream: it builds an LL(1) parsing table and a recursive-descent
parser directly from a formal grammar, constructs a parse tree, and detects and
recovers from syntax errors instead of stopping at the first one.

Written entirely in C99 — the analyzer and its frontend alike — with no external
libraries. If you have a C compiler, you can build and run the whole project.

No lexer is included by design — the analyzer's input is a token stream, not
source text. See [Token-stream format](#token-stream-format) for how to write
one.

**Repo:** https://github.com/mahimaapriyadharshinis/Parse_Bench

## Contents

- [Features](#features)
- [Getting started](#getting-started)
- [Grammar](#grammar) (full reference: [GRAMMAR.md](GRAMMAR.md))
- [How it works](#how-it-works)
- [The terminal UI](#the-terminal-ui)
- [Command line](#command-line)
- [Token-stream format](#token-stream-format)
- [Project structure](#project-structure)
- [Testing](#testing)

## Features

- **Algorithmic grammar analysis** — FIRST sets, FOLLOW sets, and the LL(1)
  parsing table are computed by generic fixed-point algorithms over bitsets,
  not hand-filled, and the construction proves the grammar is genuinely LL(1)
  (75 conflict-free table entries)
- **Recursive-descent parser** — one function per grammar rule, with
  human-readable error messages instead of raw token dumps
- **Two syntax-error recovery strategies** working together — phrase-level (a
  token is simply missing) and panic-mode (a token can't start anything valid),
  so one run reports every mistake instead of stopping at the first
- **Full parse-tree construction**, colour-coded by token category, exportable
  as a Graphviz `.dot` diagram
- **A step-by-step visualizer** that replays the parser's own execution trace —
  play/pause, single-step, scrub, the current token highlighted, tree nodes
  revealed in the exact order the algorithm built them
- **A full-screen terminal UI in plain C** — no curses, no dependencies, just
  ANSI escape sequences; all the Windows/POSIX differences are confined to one
  small file ([src/term.c](src/term.c))
- **21 automated tests** covering valid programs, syntax errors, and edge cases
  (including a watchdog-guarded regression test for a panic-mode infinite-loop
  bug that was found and fixed during development)
- **Zero dependencies** — C99 and the standard library, nothing else

## Getting started

All you need is a C compiler. On Windows, `winget install
BrechtSanders.WinLibs.POSIX.UCRT` installs one (gcc + `mingw32-make`); open a
**new** terminal afterwards so it lands on your PATH.

> **On Windows, type `mingw32-make` wherever this README says `make`** —
> MinGW-w64 ships its make under that name. On Linux and macOS, `make` is
> `make`.

```sh
make          # build build/parsebench
make run      # build and launch the terminal UI
make test     # build and run the 21-test suite
make grammar  # print the FIRST/FOLLOW sets and the LL(1) table
make clean
```

No make at all? One command builds the whole thing:

```sh
gcc -std=c99 -O2 -Isrc src/*.c -o parsebench
```

Then run it:

```sh
build\parsebench.exe                            # terminal UI on a built-in sample
build\parsebench.exe examples\custom.tokens     # terminal UI on your own file
build\parsebench.exe --cli invalid              # batch mode: print tree + errors
build\parsebench.exe --grammar                  # FIRST/FOLLOW sets, LL(1) table
build\parsebench.exe --help
```

(On Linux and macOS those are `./build/parsebench`.)

The terminal UI needs a real terminal — run it directly, not with its output
piped or redirected to a file. Batch mode (`--cli`) pipes and redirects fine.

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

See [GRAMMAR.md](GRAMMAR.md) for the full reference: this EBNF form, the
pure-BNF form the algorithms actually run on, the terminal/non-terminal lists,
and the computed FIRST/FOLLOW sets for every rule.

## How it works

1. **Grammar** ([src/grammar.c](src/grammar.c)) — the language is defined as a
   pure-BNF grammar (left recursion removed, `*`/`?` repetition expanded into
   right-recursive rules with explicit epsilon productions). FIRST sets, FOLLOW
   sets, and the LL(1) parsing table are all computed by generic fixed-point
   algorithms — nothing is hand-filled — and table construction reports an
   error immediately if it finds a FIRST/FIRST or FIRST/FOLLOW conflict, which
   is how the grammar is proven LL(1). Terminal sets are `uint32_t` bitsets, so
   a set union is one `|=` and the whole fixed-point converges in microseconds.

2. **Parsing** ([src/parser.c](src/parser.c)) — a recursive-descent parser with
   one function per non-terminal, and human-readable error messages (e.g.
   `missing ';' - found 'print' instead`, not a raw token-type dump). Two
   error-recovery strategies work together:

   - **Phrase-level recovery** for an expected-but-missing token (e.g. a
     missing `;`): the error is reported and parsing continues as if the token
     were there.
   - **Panic-mode recovery** for a token that can't start any valid construct:
     the error is reported and tokens are skipped until one in
     FIRST(statement) is found — or a `}` that actually matches the block
     currently being closed. A `}` with nothing open to close is itself skipped
     as garbage rather than treated as a safe stopping point; earlier versions
     treated any `}` as safe, which could loop forever on a stray unmatched
     brace (fixed, and covered by a watchdog-guarded regression test).

   Where the earlier Python version raised an exception to abandon a hopeless
   statement, the C version does the same non-local unwind with
   `setjmp`/`longjmp`: each active statement sequence pushes a landing pad, and
   a failure jumps to the nearest one. That abandons half-built subtrees, which
   is exactly why parse-tree nodes come from an arena instead of being
   individually owned — one `arena_destroy` reclaims the tree and every orphan
   with it.

3. **Parse tree** ([src/parse_tree.c](src/parse_tree.c)) — an n-ary tree is
   built as parsing proceeds, with recovered-error nodes flagged. It can be
   printed as an indented outline or written as a Graphviz `.dot` file. Unlike
   the earlier Python version this needs no Graphviz library to run — only the
   `dot` binary, and only if you want a picture:

   ```sh
   ./build/parsebench --cli --dot valid
   dot -Tpng parse_tree_valid.dot -o parse_tree_valid.png
   ```

4. **Watch it happen** — every parse-tree node records the order in which the
   parser created it, and the parser records which token it was looking at each
   time. The terminal UI replays that trace, so you can watch the tree being
   built node by node against the token stream.

## The terminal UI

`./build/parsebench` opens a full-screen UI with four tabs.

```
 Parse Bench  valid
 1 Analyze   2 Walkthrough   3 Grammar   4 Input
┌─ Token stream ───────────────┐┌─ Parse tree ─────────────────────────────────┐
│   0 INT     int        L1    ││ program                                      │
│   1 ID      x          L1    ││ ├─ declStmt                                  │
│   2 SEMI    ;          L1    ││ │  ├─ int                                    │
│   3 ID      x          L2    ││ │  ├─ x                                      │
│   4 ASSIGN  =          L2    ││ │  └─ ;                                      │
│   5 NUM     1          L2    ││ └─ assignStmt                                │
│   6 PLUS    +          L2    ││    ├─ x                                      │
│   7 NUM     2          L2    ││    ├─ =                                      │
│   8 STAR    *          L2    ││    └─ expr                                   │
│   9 NUM     3          L2    ││       ├─ term                                │
│  10 SEMI    ;          L2    ││       │  └─ factor                           │
│  11 PRINT   print      L3    ││       │     └─ 1                             │
└──────────────────────────────┘│       └─ +                                   │
┌─ Errors ─────────────────────┐│                                              │
│ No syntax errors.            ││                                              │
└──────────────────────────────┘└──────────────────────────────────────────────┘
 step 13/29 [paused]  █████████████████████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
```

| Tab | What it shows |
|---|---|
| **1 Analyze** | The token stream, the finished parse tree, and every recovered error |
| **2 Walkthrough** | The same panes, replayed step by step as the parser built them |
| **3 Grammar** | All 36 BNF productions, both FIRST and FOLLOW sets, and all 75 LL(1) table entries |
| **4 Input** | Load a built-in sample or open a token-stream file |

| Key | Action |
|---|---|
| `1`–`4`, `Tab` | Switch tabs |
| `↑` `↓` `PgUp` `PgDn` | Scroll |
| `←` `→` | Step back / forward (Walkthrough), or switch pane (Analyze) |
| `space` | Play / pause the walkthrough |
| `Home` `End` | Jump to the first / last step |
| `r` | Reset the walkthrough |
| `d` | Export the parse tree as `parse_tree_<name>.dot` |
| `o` | Open a token-stream file (Input tab) |
| `q` / `Esc` | Quit |

## Command line

```
parsebench                    interactive terminal UI on a built-in sample
parsebench FILE               interactive terminal UI on a token-stream file
parsebench --cli [NAMES...]   run built-in samples, print the results
parsebench --cli --file FILE  run a token-stream file, print the results
parsebench --cli --stdin      read a token stream from stdin
parsebench --cli --dot ...    also write parse_tree_<name>.dot
parsebench --grammar          print FIRST/FOLLOW sets and the LL(1) table
parsebench --help
```

Built-in samples: `valid`, `valid_if`, `invalid`, `empty`.

With `--file` or `--stdin`, batch mode exits non-zero when the input had syntax
errors, so it drops straight into a script or a CI check. (Running the built-in
samples always exits zero — one of them is *meant* to have errors.)

## Token-stream format

One or more tokens per line, whitespace-separated; `#` starts a comment. Each
token is written as `TYPE` or `TYPE(lexeme)`, where `TYPE` is a token-type name
(case-insensitive). The lexeme is optional for token types with a fixed
spelling — keywords, operators, punctuation — but **required** for `ID` and
`NUM`, since those vary. Each source line becomes one line number in error
messages.

```
INT ID(count) SEMI                                   # int count;
ID(count) ASSIGN NUM(0) SEMI                         # count = 0;

WHILE LPAREN ID(count) LT NUM(5) RPAREN LBRACE       # while (count < 5) {
    PRINT LPAREN ID(count) RPAREN SEMI               #   print(count);
    ID(count) ASSIGN ID(count) PLUS NUM(1) SEMI      #   count = count + 1;
RBRACE                                               # }
```

The valid type names are the `TokenType` enum in [src/token.h](src/token.h):
`INT IF ELSE WHILE PRINT ID NUM ASSIGN PLUS MINUS STAR SLASH LT GT LE GE EQ NE
LPAREN RPAREN LBRACE RBRACE SEMI EOF`.

See [examples/custom.tokens](examples/custom.tokens) for a complete file.

## Project structure

```
src/
  token.h/.c        TokenType, Token, and the token-stream text reader
  grammar.h/.c      the BNF grammar, FIRST/FOLLOW, and the LL(1) table
  parse_tree.h/.c   the arena, the n-ary tree, and the text/DOT renderers
  parser.h/.c       the recursive-descent parser and error recovery
  samples.h         the built-in demo token streams
  term.h/.c         raw-mode keys, screen size, ANSI colours (the only
                    platform-specific file: Windows and POSIX)
  tui.h/.c          the terminal UI
  main.c            argument handling and batch mode
tests/
  minitest.h        a ~60-line test harness, so no framework is needed
  watchdog.h/.c     a wall-clock timeout for the infinite-loop regression test
  test_main.c       all 21 tests
examples/
  custom.tokens     a sample token-stream file
Makefile
```

Everything except `src/term.c` and `tests/watchdog.c` is portable C99 with no
`#ifdef` in sight.

## Testing

```sh
make test
```

```
ok    test_decl_assign_print_has_no_errors
ok    test_if_else_has_no_errors
...
ok    test_full_custom_stream_parses_cleanly_end_to_end

21 tests, 0 failed
```

The suite covers four areas:

- **Valid programs** (4) — declarations, assignments, `if`/`else`, `while`,
  nested parenthesized expressions
- **Syntax errors** (5) — each recovery path, including several independent
  errors reported in a single pass
- **Edge cases** (6) — empty input, empty blocks, 20-deep parenthesization,
  15-deep block nesting, the stray-`}` infinite-loop regression, and the
  LL(1) table's own conflict check
- **Token-stream format** (6) — default lexemes, line-number tracking,
  comments, and both error cases

The stray-`}` test is guarded by a watchdog thread: that bug made the parser
spin forever rather than return a wrong answer, so the test arms a five-second
timeout and kills the process with a clear message if the parse ever hangs
again.
