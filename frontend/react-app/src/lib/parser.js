/**
 * Recursive-descent LL(1) parser with parse-tree construction and combined
 * panic-mode / phrase-level syntax error recovery. Ported 1:1 from the
 * Python parser.py, non-terminal for non-terminal (including the friendly
 * error-message phrasing).
 *
 * - "Soft" expect() -- used for an expected closing/terminating token (a
 *   missing ';' or ')'). Reports the error but does NOT raise: pretends the
 *   token was there (phrase-level recovery) and keeps parsing from the
 *   current token.
 * - "Hard" dispatch failure -- thrown as a ParseError when the current
 *   token doesn't match ANY alternative of a non-terminal. Caught by
 *   parseStmtSequence, which runs synchronize() (panic-mode recovery): skip
 *   tokens until one in FOLLOW(statement) (computed in grammar.js) is found.
 *
 * Every node created also gets a stable `_id`, and every meaningful step
 * (a rule completing, a token being consumed, an error, a panic-mode skip)
 * is appended to `this.trace` -- this costs nothing extra for normal use,
 * and is what lets the UI's step-by-step Walkthrough tab replay the exact
 * order the algorithm actually worked in.
 */

import { FIRST } from "./grammar.js";

const FIRST_STATEMENT = new Set([...FIRST["statement"]].filter((t) => t !== "EPSILON"));
const RELOPS = new Set(["LT", "GT", "LE", "GE", "EQ", "NE"]);

const FRIENDLY_TYPE = {
  SEMI: "';'", RPAREN: "')'", LPAREN: "'('", RBRACE: "'}'", LBRACE: "'{'",
  ASSIGN: "'='", ID: "an identifier", NUM: "a number", INT: "'int'",
  ELSE: "'else'", EOF: "the end of the file",
};

function friendlyType(ttype) {
  return FRIENDLY_TYPE[ttype] || ttype;
}
function friendlyToken(tok) {
  if (tok.type === "EOF") return "the end of the file";
  return `'${tok.lexeme}'`;
}

export class ParseError extends Error {
  /** `missingToken` set => phrasing is "missing X" (from a soft expect());
   * left undefined => `expectedDesc` is already a full human phrase, used
   * verbatim (a hard dispatch failure, e.g. "start of statement"). */
  constructor(token, expectedDesc, missingToken) {
    const found = friendlyToken(token);
    const message = missingToken
      ? `Line ${token.line}: missing ${friendlyType(missingToken)} - found ${found} instead`
      : `Line ${token.line}: expected ${expectedDesc}, but found ${found}`;
    super(message);
    this.token = token;
    this.expectedDesc = expectedDesc;
  }
}

export function node(label, children, isError) {
  return { label, children: children || [], isError: !!isError, _id: -1 };
}

export class Parser {
  constructor(tokens) {
    this.tokens = tokens;
    this.pos = 0;
    this.errors = [];
    this.trace = [];
    this._nextId = 0;
  }

  current() { return this.tokens[this.pos]; }
  check(t) { return this.current().type === t; }
  advance() {
    const tok = this.tokens[this.pos];
    if (tok.type !== "EOF") this.pos++;
    return tok;
  }
  report(err) { this.errors.push(err.message); }

  /** Creates a node with a fresh id (does NOT reveal/trace it yet). */
  makeNode(label, children, isError) {
    const n = node(label, children, isError);
    n._id = this._nextId++;
    return n;
  }

  /** A non-terminal's node is fully built -- log it as a completed match. */
  finish(n) {
    this.trace.push({ type: "match", nodeId: n._id, label: n.label, pos: this.pos });
    return n;
  }

  /** A single real token becomes a leaf node -- log it as consumed. The
   * token is attached to the node (JS-rendering metadata only, no Python
   * equivalent needed) so the UI can color it by category later. */
  leaf(tok) {
    const n = this.makeNode(tok.lexeme || tok.type);
    n.token = tok;
    this.trace.push({ type: "consume", nodeId: n._id, token: tok, pos: this.pos });
    return n;
  }

  expect(ttype) {
    if (this.check(ttype)) return this.leaf(this.advance());
    const err = new ParseError(this.current(), friendlyType(ttype), ttype);
    this.report(err);
    const n = this.makeNode(`<missing ${ttype}>`, [], true);
    this.trace.push({ type: "error", nodeId: n._id, message: err.message, pos: this.pos });
    return n;
  }

  /** Panic-mode recovery: skip tokens until one in FIRST(statement) is
   * found, or SEMI (consumed), or a RBRACE that actually matches the block
   * this call is inside (i.e. equals `terminator`). A RBRACE that ISN'T the
   * active terminator is a stray/unmatched '}' with nothing open to close --
   * it must be skipped like any other garbage token, not treated as a safe
   * stopping point, or a top-level stray '}' would never be consumed and
   * this would loop forever. */
  synchronize(terminator) {
    while (!this.check("EOF")) {
      if (this.check("SEMI")) {
        const t = this.advance();
        this.trace.push({ type: "resync", token: t, pos: this.pos });
        return;
      }
      if (this.check("RBRACE")) {
        if (this.current().type === terminator) {
          this.trace.push({ type: "resync", token: this.current(), pos: this.pos });
          return;
        }
        this.trace.push({ type: "skip", token: this.current(), pos: this.pos });
        this.advance();
        continue;
      }
      if (FIRST_STATEMENT.has(this.current().type)) {
        this.trace.push({ type: "resync", token: this.current(), pos: this.pos });
        return;
      }
      this.trace.push({ type: "skip", token: this.current(), pos: this.pos });
      this.advance();
    }
    this.trace.push({ type: "resync", token: this.current(), pos: this.pos });
  }

  parse() {
    const n = this.makeNode("program");
    this.parseStmtSequence(n, "EOF");
    return this.finish(n);
  }

  parseStmtSequence(n, terminator) {
    while (!this.check(terminator) && !this.check("EOF")) {
      if (!FIRST_STATEMENT.has(this.current().type)) {
        const err = new ParseError(this.current(), "start of statement");
        this.report(err);
        const skipped = this.makeNode(`<skipped '${this.current().lexeme}'>`, [], true);
        this.trace.push({ type: "error", nodeId: skipped._id, message: err.message, pos: this.pos });
        n.children.push(skipped);
        this.synchronize(terminator);
        continue;
      }
      try {
        n.children.push(this.parseStatement());
      } catch (err) {
        if (!(err instanceof ParseError)) throw err;
        this.report(err);
        const errNode = this.makeNode(`<error: ${err.message}>`, [], true);
        this.trace.push({ type: "error", nodeId: errNode._id, message: err.message, pos: this.pos });
        n.children.push(errNode);
        this.synchronize(terminator);
      }
    }
  }

  parseStatement() {
    const t = this.current().type;
    if (t === "INT") return this.parseDeclStmt();
    if (t === "ID") return this.parseAssignStmt();
    if (t === "IF") return this.parseIfStmt();
    if (t === "WHILE") return this.parseWhileStmt();
    if (t === "LBRACE") return this.parseBlock();
    if (t === "PRINT") return this.parsePrintStmt();
    throw new ParseError(this.current(), "start of statement");
  }

  parseDeclStmt() {
    const n = this.makeNode("declStmt");
    n.children.push(this.leaf(this.advance()));
    n.children.push(this.expect("ID"));
    n.children.push(this.expect("SEMI"));
    return this.finish(n);
  }

  parseAssignStmt() {
    const n = this.makeNode("assignStmt");
    n.children.push(this.leaf(this.advance()));
    n.children.push(this.expect("ASSIGN"));
    n.children.push(this.parseExpr());
    n.children.push(this.expect("SEMI"));
    return this.finish(n);
  }

  parseIfStmt() {
    const n = this.makeNode("ifStmt");
    n.children.push(this.leaf(this.advance()));
    n.children.push(this.expect("LPAREN"));
    n.children.push(this.parseCond());
    n.children.push(this.expect("RPAREN"));
    n.children.push(this.parseBlock());
    n.children.push(this.parseElsePart());
    return this.finish(n);
  }

  parseElsePart() {
    if (this.check("ELSE")) {
      const n = this.makeNode("elsePart");
      n.children.push(this.leaf(this.advance()));
      n.children.push(this.parseBlock());
      return this.finish(n);
    }
    return this.finish(this.makeNode("elsePart(ε)"));
  }

  parseWhileStmt() {
    const n = this.makeNode("whileStmt");
    n.children.push(this.leaf(this.advance()));
    n.children.push(this.expect("LPAREN"));
    n.children.push(this.parseCond());
    n.children.push(this.expect("RPAREN"));
    n.children.push(this.parseBlock());
    return this.finish(n);
  }

  parseBlock() {
    const n = this.makeNode("block");
    n.children.push(this.expect("LBRACE"));
    this.parseStmtSequence(n, "RBRACE");
    n.children.push(this.expect("RBRACE"));
    return this.finish(n);
  }

  parsePrintStmt() {
    const n = this.makeNode("printStmt");
    n.children.push(this.leaf(this.advance()));
    n.children.push(this.expect("LPAREN"));
    n.children.push(this.parseExpr());
    n.children.push(this.expect("RPAREN"));
    n.children.push(this.expect("SEMI"));
    return this.finish(n);
  }

  parseCond() {
    const n = this.makeNode("cond");
    n.children.push(this.parseExpr());
    n.children.push(this.parseRelop());
    n.children.push(this.parseExpr());
    return this.finish(n);
  }

  parseRelop() {
    if (RELOPS.has(this.current().type)) {
      return this.finish(this.makeNode("relop", [this.leaf(this.advance())]));
    }
    throw new ParseError(this.current(), "relational operator (< > <= >= == !=)");
  }

  parseExpr() {
    const n = this.makeNode("expr");
    n.children.push(this.parseTerm());
    while (this.current().type === "PLUS" || this.current().type === "MINUS") {
      n.children.push(this.leaf(this.advance()));
      n.children.push(this.parseTerm());
    }
    return this.finish(n);
  }

  parseTerm() {
    const n = this.makeNode("term");
    n.children.push(this.parseFactor());
    while (this.current().type === "STAR" || this.current().type === "SLASH") {
      n.children.push(this.leaf(this.advance()));
      n.children.push(this.parseFactor());
    }
    return this.finish(n);
  }

  parseFactor() {
    if (this.current().type === "ID" || this.current().type === "NUM") {
      return this.finish(this.makeNode("factor", [this.leaf(this.advance())]));
    }
    if (this.check("LPAREN")) {
      const n = this.makeNode("factor");
      n.children.push(this.leaf(this.advance()));
      n.children.push(this.parseExpr());
      n.children.push(this.expect("RPAREN"));
      return this.finish(n);
    }
    throw new ParseError(this.current(), "identifier, number, or '('");
  }
}

export function parse(tokens) {
  const parser = new Parser(tokens);
  const tree = parser.parse();
  return { tree, errors: parser.errors, trace: parser.trace };
}

export function countNodesAndDepth(n, depth = 0) {
  let count = 1;
  let maxDepth = depth;
  for (const c of n.children) {
    const r = countNodesAndDepth(c, depth + 1);
    count += r.count;
    maxDepth = Math.max(maxDepth, r.maxDepth);
  }
  return { count, maxDepth };
}
