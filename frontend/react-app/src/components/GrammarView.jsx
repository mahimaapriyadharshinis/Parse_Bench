import { GRAMMAR, EPSILON, FIRST, FOLLOW, LL1 } from "../lib/grammar.js";

const RULE_ORDER = [
  "program", "statement", "declStmt", "assignStmt", "ifStmt", "elsePart", "whileStmt",
  "block", "stmtList", "printStmt", "cond", "relop", "expr", "exprTail", "term", "termTail", "factor",
];

function GrammarRules() {
  return (
    <pre className="rules">
      {RULE_ORDER.map((nt) => {
        const alts = GRAMMAR[nt].map((prod) => prod.map((s) => (s === EPSILON ? "ε" : s)).join(" ")).join(" | ");
        return (
          <div key={nt}>
            <span className="nt">{nt.padEnd(11)}</span>
            <span className="op">-&gt;</span> {alts}
          </div>
        );
      })}
    </pre>
  );
}

function SetsTable() {
  const order = Object.keys(GRAMMAR);
  return (
    <div className="table-scroll">
      <table className="sets">
        <thead>
          <tr>
            <th>Non-terminal</th>
            <th>FIRST</th>
            <th>FOLLOW</th>
          </tr>
        </thead>
        <tbody>
          {order.map((nt) => (
            <tr key={nt}>
              <td className="nt">{nt}</td>
              <td className="set">{`{ ${[...FIRST[nt]].map((t) => (t === EPSILON ? "ε" : t)).join(", ")} }`}</td>
              <td className="set">{`{ ${[...FOLLOW[nt]].join(", ")} }`}</td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}

export default function GrammarView() {
  const ok = LL1.conflicts.length === 0;
  return (
    <div>
      <div
        className="grammar-status"
        style={{
          background: ok ? "var(--success-soft)" : "var(--error-soft)",
          color: ok ? "var(--success)" : "var(--error)",
        }}
      >
        {ok ? (
          <>
            <svg viewBox="0 0 24 24" fill="none">
              <path d="M4 12.5 9.5 18 20 6" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" />
            </svg>
            Grammar confirmed LL(1) — {Object.keys(LL1.table).length} table entries, 0 conflicts
          </>
        ) : (
          `${LL1.conflicts.length} FIRST/FOLLOW conflict(s) found — grammar is not LL(1)`
        )}
      </div>
      <h2 style={{ marginBottom: 10 }}>Grammar (pure BNF)</h2>
      <GrammarRules />
      <h2 style={{ marginBottom: 10 }}>FIRST / FOLLOW sets</h2>
      <SetsTable />
    </div>
  );
}
