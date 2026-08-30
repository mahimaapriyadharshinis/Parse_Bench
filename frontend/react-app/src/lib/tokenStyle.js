/** Shared token -> syntax-highlight category mapping, used by the token
 * stream view, the parse tree, and the walkthrough -- one classification,
 * reused everywhere a token needs a color, matching how a real editor
 * colors a token stream by category rather than one decorative brand hue. */

const KEYWORD_TYPES = new Set(["INT", "IF", "ELSE", "WHILE", "PRINT"]);

export function tokenCategory(type) {
  if (KEYWORD_TYPES.has(type)) return "keyword";
  if (type === "ID") return "ident";
  if (type === "NUM") return "num";
  if (type === "EOF") return "eof";
  return "op";
}

export const CATEGORY_COLOR_VAR = {
  keyword: "var(--syn-keyword)",
  ident: "var(--syn-ident)",
  num: "var(--syn-num)",
  op: "var(--syn-op)",
  eof: "var(--text-faint)",
};
