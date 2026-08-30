export default function ErrorsView({ errors }) {
  if (!errors.length) {
    return (
      <div className="empty-state success">
        <svg viewBox="0 0 24 24" fill="none">
          <path d="M4 12.5 9.5 18 20 6" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" />
        </svg>
        <div className="big">No syntax errors detected</div>
        <div>Every statement matched the grammar on the first try.</div>
      </div>
    );
  }

  return (
    <div>
      {errors.map((e, i) => {
        const m = /^Line (\d+): (.*)$/.exec(e);
        return (
          <div className="error-card" key={i}>
            <div className="dot" />
            <div>
              <div className="line">
                Recovered error {i + 1} · line {m ? m[1] : "?"}
              </div>
              <div className="msg">{m ? m[2] : e}</div>
            </div>
          </div>
        );
      })}
    </div>
  );
}
