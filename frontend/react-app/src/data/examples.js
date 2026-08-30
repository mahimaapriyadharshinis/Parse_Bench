export const EXAMPLES = {
  valid: {
    label: "Valid program",
    text: `INT ID(count) SEMI
ID(count) ASSIGN NUM(0) SEMI

WHILE LPAREN ID(count) LT NUM(5) RPAREN LBRACE
    PRINT LPAREN ID(count) RPAREN SEMI
    ID(count) ASSIGN ID(count) PLUS NUM(1) SEMI
RBRACE`,
  },
  broken: {
    label: "Has errors",
    text: `INT ID(x) SEMI
ID(x) ASSIGN NUM(1) PLUS NUM(2)
PRINT LPAREN ID(x) SEMI`,
  },
  empty: {
    label: "Empty",
    text: `# an empty token stream is a valid (trivial) program`,
  },
};
