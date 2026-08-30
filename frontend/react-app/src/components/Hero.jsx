export default function Hero({ tokenCount, nodeCount, maxDepth, errorCount, hasTokens }) {
  const good = errorCount === 0;
  const statusText = !hasTokens ? "EMPTY" : good ? "VALID" : `${errorCount} ERROR${errorCount > 1 ? "S" : ""}`;

  return (
    <div className="hero">
      <div className="hero-grid">
        <div className="stat">
          <span className="n">{tokenCount ?? "—"}</span>
          <span className="l">tokens</span>
        </div>
        <div className="stat">
          <span className="n">{nodeCount ?? "—"}</span>
          <span className="l">nodes</span>
        </div>
        <div className="stat">
          <span className="n">{maxDepth ?? "—"}</span>
          <span className="l">depth</span>
        </div>
        <div className="stat errors">
          <span className="n">{errorCount ?? "—"}</span>
          <span className="l">recovered</span>
        </div>
        <span className={`status-badge${good ? "" : " bad"}`}>
          <span className="dot" />
          {statusText}
        </span>
      </div>
    </div>
  );
}
