"""CLI demo: run the parser over sample OR user-defined token streams.

Usage:
    py main.py                       run all built-in samples
    py main.py valid invalid         run specific built-in samples by name
    py main.py --file mytokens.txt   parse a user-defined token-stream file
    py main.py --stdin               read a user-defined token stream from stdin

See tokens.py's "User-defined token streams" section for the text format,
or examples/custom.tokens for a full sample file.
"""

import argparse
import sys

import tokens as tok_mod
from parser import parse
from parse_tree import print_tree, to_graphviz

SAMPLES = {
    "valid": tok_mod.SAMPLE_VALID,
    "valid_if": tok_mod.SAMPLE_VALID_IF,
    "invalid": tok_mod.SAMPLE_INVALID,
    "empty": tok_mod.SAMPLE_EMPTY,
}


def run(name: str, stream, export_graphviz: bool = True) -> None:
    print(f"\n=== {name} ===")
    tree, errors = parse(stream)

    print("-- Parse tree --")
    print_tree(tree)

    if errors:
        print(f"\n-- {len(errors)} syntax error(s) detected (recovered) --")
        for e in errors:
            print(f"  {e}")
    else:
        print("\n-- No syntax errors --")

    if export_graphviz:
        try:
            to_graphviz(tree, filename=f"parse_tree_{name}")
            print(f"\nGraphviz export written to parse_tree_{name}.png")
        except ImportError as exc:
            print(f"\n(Graphviz export skipped: {exc})")
        except Exception as exc:
            print(
                f"\n(Graphviz export skipped: Graphviz system binary not found "
                f"({exc}). Install it from https://graphviz.org/download/ "
                f"and ensure 'dot' is on PATH.)"
            )


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("names", nargs="*", help="built-in sample names to run")
    ap.add_argument("--file", metavar="PATH", help="parse a user-defined token-stream file")
    ap.add_argument("--stdin", action="store_true", help="read a user-defined token stream from stdin")
    args = ap.parse_args()

    if args.file:
        try:
            stream = tok_mod.parse_token_stream_file(args.file)
        except (OSError, tok_mod.TokenStreamFormatError) as exc:
            print(f"Error reading '{args.file}': {exc}")
            sys.exit(1)
        run(args.file, stream)
        return

    if args.stdin:
        try:
            stream = tok_mod.parse_token_stream_text(sys.stdin.read())
        except tok_mod.TokenStreamFormatError as exc:
            print(f"Error reading stdin: {exc}")
            sys.exit(1)
        run("stdin", stream)
        return

    names = args.names or list(SAMPLES.keys())
    for n in names:
        if n not in SAMPLES:
            print(f"Unknown sample '{n}'. Choices: {list(SAMPLES.keys())}")
            continue
        run(n, SAMPLES[n])


if __name__ == "__main__":
    main()
