# Parse Bench (React)

A live LL(1) recursive-descent syntax analyzer, built as a real Vite + React app. See
the [project root README](../../README.md) for the full picture — this is one of two
frontends there (the other is the zero-install `frontend/parse_bench.html`).

## Run it

```
npm install
npm run dev
```

Open the URL it prints — use `http://localhost:5173`, not `http://127.0.0.1:5173`
(Vite's dev server binds to the IPv6 loopback address on some machines; only the
`localhost` hostname resolves to it there).

```
npm run build     # production build, output in dist/
npm run preview   # serve the production build locally
```

## Structure

```
src/
  lib/            Grammar, FIRST/FOLLOW/LL(1) table, token-stream parser, and the
                   recursive-descent parser itself — a 1:1 port of the project's
                   grammar.py / tokens.py / parser.py, not a reimplementation.
                   Also records every parse step (a rule matched, a token consumed,
                   an error, a panic-mode skip) into a trace array, and maps each
                   token to a syntax-highlight category (tokenStyle.js).
  hooks/          useTreeZoom.js — auto-fits a tree to its container and exposes
                   zoom in/out/fit, shared by the tree tab and the walkthrough tab.
  components/     UI: input panel, status line, and a tabbed view — Walkthrough
                   (the default tab: replays the trace with Play/Pause/step/scrub),
                   ParseTreeView, TokensView, GrammarView, ErrorsView.
  data/examples.js
  App.jsx
```

Everything downstream of your typed token stream — FIRST/FOLLOW sets, the LL(1) table,
the parse tree, the recovered errors — is computed live in the browser. Nothing is
precomputed or hardcoded.
