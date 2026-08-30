/**
 * Plain-text token-stream format, ported 1:1 from the Python
 * tokens.py's parse_token_stream_text().
 *
 * One or more tokens per line, whitespace-separated. Each token is written
 * TYPE or TYPE(lexeme); '#' starts a comment. A lexeme is required for
 * ID/NUM and optional (has a default) for every other type.
 */

export const VALID_TYPES = new Set([
  "INT", "IF", "ELSE", "WHILE", "PRINT", "ID", "NUM", "ASSIGN", "PLUS", "MINUS",
  "STAR", "SLASH", "LT", "GT", "LE", "GE", "EQ", "NE", "LPAREN", "RPAREN",
  "LBRACE", "RBRACE", "SEMI", "EOF",
]);

export const DEFAULT_LEXEME = {
  INT: "int", IF: "if", ELSE: "else", WHILE: "while", PRINT: "print",
  ASSIGN: "=", PLUS: "+", MINUS: "-", STAR: "*", SLASH: "/",
  LT: "<", GT: ">", LE: "<=", GE: ">=", EQ: "==", NE: "!=",
  LPAREN: "(", RPAREN: ")", LBRACE: "{", RBRACE: "}", SEMI: ";", EOF: "",
};

const TOKEN_SPEC_RE = /^([A-Za-z_]+)(?:\((.*)\))?$/;

export class TokenStreamFormatError extends Error {}

function parseTokenSpec(piece, lineNo) {
  const match = TOKEN_SPEC_RE.exec(piece);
  if (!match) {
    throw new TokenStreamFormatError(
      `Line ${lineNo}: cannot parse token spec '${piece}' (expected TYPE or TYPE(lexeme))`
    );
  }
  const name = match[1].toUpperCase();
  let lexeme = match[2];
  if (!VALID_TYPES.has(name)) {
    throw new TokenStreamFormatError(`Line ${lineNo}: unknown token type '${name}' in '${piece}'`);
  }
  if (lexeme === undefined) {
    lexeme = DEFAULT_LEXEME[name];
    if (lexeme === undefined) {
      throw new TokenStreamFormatError(
        `Line ${lineNo}: token type ${name} needs an explicit lexeme, e.g. ${name}(x)`
      );
    }
  }
  return { type: name, lexeme, line: lineNo };
}

export function parseTokenStreamText(text) {
  const tokens = [];
  const lines = text.split("\n");
  for (let i = 0; i < lines.length; i++) {
    const lineNo = i + 1;
    const line = lines[i].split("#")[0];
    for (const piece of line.split(/\s+/).filter(Boolean)) {
      tokens.push(parseTokenSpec(piece, lineNo));
    }
  }
  if (!tokens.length || tokens[tokens.length - 1].type !== "EOF") {
    const lastLine = tokens.length ? tokens[tokens.length - 1].line : 1;
    tokens.push({ type: "EOF", lexeme: "", line: lastLine });
  }
  return tokens;
}
