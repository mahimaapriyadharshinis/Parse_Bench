/**
 * Grammar definition (pure BNF, no left recursion) plus generic
 * FIRST/FOLLOW/LL(1)-table computation. This is a 1:1 port of the Python
 * grammar.py used by the actual compiler-design submission -- terminals are
 * TokenType-name strings, non-terminals are the keys of GRAMMAR.
 */

export const EPSILON = "EPSILON";
export const END = "EOF";

export const GRAMMAR = {
  program:    [["statement", "program"], [EPSILON]],
  statement:  [["declStmt"], ["assignStmt"], ["ifStmt"], ["whileStmt"], ["block"], ["printStmt"]],
  declStmt:   [["INT", "ID", "SEMI"]],
  assignStmt: [["ID", "ASSIGN", "expr", "SEMI"]],
  ifStmt:     [["IF", "LPAREN", "cond", "RPAREN", "block", "elsePart"]],
  elsePart:   [["ELSE", "block"], [EPSILON]],
  whileStmt:  [["WHILE", "LPAREN", "cond", "RPAREN", "block"]],
  block:      [["LBRACE", "stmtList", "RBRACE"]],
  stmtList:   [["statement", "stmtList"], [EPSILON]],
  printStmt:  [["PRINT", "LPAREN", "expr", "RPAREN", "SEMI"]],
  cond:       [["expr", "relop", "expr"]],
  relop:      [["LT"], ["GT"], ["LE"], ["GE"], ["EQ"], ["NE"]],
  expr:       [["term", "exprTail"]],
  exprTail:   [["PLUS", "term", "exprTail"], ["MINUS", "term", "exprTail"], [EPSILON]],
  term:       [["factor", "termTail"]],
  termTail:   [["STAR", "factor", "termTail"], ["SLASH", "factor", "termTail"], [EPSILON]],
  factor:     [["ID"], ["NUM"], ["LPAREN", "expr", "RPAREN"]],
};

export const START_SYMBOL = "program";

export const isTerminal = (sym) => !(sym in GRAMMAR) && sym !== EPSILON;

function firstOfSequence(seq, first) {
  const result = new Set();
  for (const sym of seq) {
    if (sym === EPSILON) { result.add(EPSILON); return result; }
    if (isTerminal(sym)) { result.add(sym); return result; }
    for (const t of first[sym]) if (t !== EPSILON) result.add(t);
    if (!first[sym].has(EPSILON)) return result;
  }
  result.add(EPSILON);
  return result;
}

export function computeFirstSets(grammar) {
  const first = {};
  for (const nt in grammar) first[nt] = new Set();
  let changed = true;
  while (changed) {
    changed = false;
    for (const nt in grammar) {
      for (const prod of grammar[nt]) {
        const before = first[nt].size;
        for (const t of firstOfSequence(prod, first)) first[nt].add(t);
        if (first[nt].size !== before) changed = true;
      }
    }
  }
  return first;
}

export function computeFollowSets(grammar, first, startSymbol) {
  const follow = {};
  for (const nt in grammar) follow[nt] = new Set();
  follow[startSymbol].add(END);
  let changed = true;
  while (changed) {
    changed = false;
    for (const nt in grammar) {
      for (const prod of grammar[nt]) {
        for (let i = 0; i < prod.length; i++) {
          const sym = prod[i];
          if (isTerminal(sym) || sym === EPSILON) continue;
          const rest = prod.slice(i + 1);
          const restFirst = rest.length ? firstOfSequence(rest, first) : new Set([EPSILON]);
          const before = follow[sym].size;
          for (const t of restFirst) if (t !== EPSILON) follow[sym].add(t);
          if (restFirst.has(EPSILON)) for (const t of follow[nt]) follow[sym].add(t);
          if (follow[sym].size !== before) changed = true;
        }
      }
    }
  }
  return follow;
}

/**
 * Builds the LL(1) parsing table and collects any FIRST/FIRST or
 * FIRST/FOLLOW conflicts instead of throwing, so the UI can display the
 * grammar's LL(1) status either way.
 */
export function buildLL1Table(grammar, first, follow) {
  const table = {};
  const conflicts = [];
  for (const nt in grammar) {
    for (const prod of grammar[nt]) {
      const prodFirst = firstOfSequence(prod, first);
      const onTerminal = (terminal) => {
        const key = nt + "|" + terminal;
        if (table[key]) conflicts.push({ nt, terminal, a: table[key], b: prod });
        else table[key] = prod;
      };
      for (const t of prodFirst) if (t !== EPSILON) onTerminal(t);
      if (prodFirst.has(EPSILON)) for (const t of follow[nt]) onTerminal(t);
    }
  }
  return { table, conflicts };
}

export const FIRST = computeFirstSets(GRAMMAR);
export const FOLLOW = computeFollowSets(GRAMMAR, FIRST, START_SYMBOL);
export const LL1 = buildLL1Table(GRAMMAR, FIRST, FOLLOW);
