function tokenClass(t) {
  if (["INT", "IF", "ELSE", "WHILE", "PRINT"].includes(t.type)) return "kw";
  if (t.type === "ID") return "lit";
  if (t.type === "NUM") return "num";
  if (t.type === "EOF") return "eof";
  return "";
}

export default function TokensView({ tokens }) {
  return (
    <div className="token-stream">
      {tokens.map((t, i) => (
        <span key={i} className={`tok ${tokenClass(t)}`}>
          <span className="ty">{t.type}</span>
          {t.lexeme || (t.type === "EOF" ? "␣" : "")}
        </span>
      ))}
    </div>
  );
}
