import { EXAMPLES } from "../data/examples.js";

export default function InputPanel({ value, onChange, onAnalyze, formatError }) {
  const handleKeyDown = (e) => {
    if (e.ctrlKey && e.key === "Enter") onAnalyze();
  };

  return (
    <div className="panel panel-pad">
      <h2>Token stream input</h2>

      <div className="field-label">
        <span>Stream text</span>
      </div>
      <textarea
        spellCheck={false}
        value={value}
        onChange={(e) => onChange(e.target.value)}
        onKeyDown={handleKeyDown}
      />

      <div className="examples">
        {Object.entries(EXAMPLES).map(([key, ex]) => (
          <button key={key} className="chip-btn" onClick={() => onChange(ex.text)}>
            {ex.label}
          </button>
        ))}
      </div>

      <button className="analyze-btn" onClick={onAnalyze}>
        Analyze token stream
      </button>
      <div className="hint">
        Runs automatically as you type &middot; or press <kbd>Ctrl</kbd>+<kbd>Enter</kbd>
      </div>

      {formatError && <div className="format-error">Input format error: {formatError}</div>}

      <details className="format">
        <summary>Token-stream format</summary>
        <div className="format-body">
          <p>
            One or more tokens per line, separated by spaces. Each token is written{" "}
            <code>TYPE</code> or <code>TYPE(lexeme)</code>. A lexeme is required for{" "}
            <code>ID</code>/<code>NUM</code>, optional elsewhere (defaults apply).{" "}
            <code>#</code> starts a comment.
          </p>
          <p>
            <code>INT ID(x) SEMI</code>
            <br />
            <code>ID(x) ASSIGN NUM(1) PLUS NUM(2) SEMI</code>
          </p>
          <p>
            Types: INT IF ELSE WHILE PRINT ID NUM ASSIGN PLUS MINUS STAR SLASH LT GT LE GE EQ NE
            LPAREN RPAREN LBRACE RBRACE SEMI
          </p>
        </div>
      </details>
    </div>
  );
}
