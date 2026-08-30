import { useEffect, useMemo, useRef, useState } from "react";
import { layoutTree } from "../lib/treeLayout.js";
import { tokenCategory, CATEGORY_COLOR_VAR } from "../lib/tokenStyle.js";
import { useTreeZoom } from "../hooks/useTreeZoom.js";
import TreeZoomControls from "./TreeZoomControls.jsx";

const SPEEDS = { slow: 900, normal: 450, fast: 150 };

function friendlyTok(t) {
  if (!t) return "";
  return t.type === "EOF" ? "end of file" : `'${t.lexeme}' (${t.type})`;
}

function describeEvent(ev) {
  switch (ev.type) {
    case "match":
      return { text: `Matched ${ev.label}`, tone: "ok" };
    case "consume":
      return { text: `Consumed ${friendlyTok(ev.token)}`, tone: "ok" };
    case "error":
      return { text: ev.message, tone: "bad" };
    case "skip":
      return { text: `Skipped ${friendlyTok(ev.token)} — can't start a statement`, tone: "bad" };
    case "resync":
      return { text: `Resumed parsing at ${friendlyTok(ev.token)}`, tone: "warn" };
    default:
      return { text: "", tone: "" };
  }
}

function TreeSvg({ layout, revealed, justRevealedId, scale }) {
  const { nodes, edges, width, height } = layout;
  return (
    <svg viewBox={`0 0 ${width} ${height}`} width={width * scale} height={height * scale} role="img" aria-label="Parse tree being built step by step">
      {edges
        .filter(([p, c]) => revealed.has(p._id) && revealed.has(c._id))
        .map(([p, c], i) => {
          const midY = (p._y + c._y) / 2;
          return <path key={i} d={`M ${p._x} ${p._y} V ${midY} H ${c._x} V ${c._y}`} fill="none" stroke="var(--border)" strokeWidth="1.4" />;
        })}
      {nodes
        .filter((n) => revealed.has(n._id))
        .map((n) => {
          const w = Math.max(28, n.label.length * 6.6 + 16);
          const h = 22;
          let fill = "var(--panel-2)", stroke = "var(--border)", tcolor = "var(--text-dim)", fam = "IBM Plex Sans";
          if (n.token) {
            const color = CATEGORY_COLOR_VAR[tokenCategory(n.token.type)];
            fill = "var(--panel)"; stroke = color; tcolor = color; fam = "IBM Plex Mono";
          } else if (n._leaf) {
            fill = "var(--panel)"; stroke = "var(--border)"; tcolor = "var(--text-faint)"; fam = "IBM Plex Mono";
          }
          if (n.isError) { fill = "var(--error-soft)"; stroke = "var(--error)"; tcolor = "var(--error)"; }
          const isNew = n._id === justRevealedId;
          return (
            <g key={n._id}>
              {isNew && (
                <rect x={n._x - w / 2 - 4} y={n._y - h / 2 - 4} width={w + 8} height={h + 8} rx="4" fill="none" stroke="var(--text)" strokeWidth="1.3" opacity="0.55" />
              )}
              <rect x={n._x - w / 2} y={n._y - h / 2} width={w} height={h} rx="3" fill={fill} stroke={stroke} strokeWidth={isNew ? 1.8 : 1.2} />
              <text x={n._x} y={n._y + 4} textAnchor="middle" fontSize="11" fontFamily={fam} fill={tcolor}>{n.label}</text>
            </g>
          );
        })}
    </svg>
  );
}

export default function Walkthrough({ tree, tokens, trace }) {
  const [step, setStep] = useState(0);
  const [playing, setPlaying] = useState(false);
  const [speed, setSpeed] = useState("normal");
  const logRef = useRef(null);

  useEffect(() => {
    if (!playing || step >= trace.length) return undefined;
    const id = setTimeout(() => setStep((s) => Math.min(s + 1, trace.length)), SPEEDS[speed]);
    return () => clearTimeout(id);
  }, [playing, step, speed, trace.length]);

  useEffect(() => {
    if (logRef.current) logRef.current.scrollTop = logRef.current.scrollHeight;
  }, [step]);

  const layout = useMemo(() => layoutTree(tree), [tree]);
  const { containerRef, scale, fit, zoomIn, zoomOut } = useTreeZoom(layout.width, layout.height);

  const revealed = useMemo(() => {
    const s = new Set();
    for (let i = 0; i < step; i++) {
      const ev = trace[i];
      if (ev.nodeId !== undefined) s.add(ev.nodeId);
    }
    return s;
  }, [step, trace]);

  const currentPos = step > 0 ? trace[step - 1].pos : 0;
  const justRevealedId = step > 0 ? trace[step - 1].nodeId : undefined;
  const done = step >= trace.length;

  if (!trace.length) {
    return <div className="empty-state"><div className="big">Nothing to walk through</div><div>Analyze a non-empty token stream first.</div></div>;
  }

  return (
    <div className="walkthrough">
      <div className="wt-controls">
        <button className="wt-btn" onClick={() => setStep(0)} title="Restart" aria-label="Restart">⟲</button>
        <button className="wt-btn" onClick={() => setStep((s) => Math.max(0, s - 1))} disabled={step === 0} aria-label="Step back">◂</button>
        <button
          className="wt-btn primary"
          onClick={() => {
            if (done) { setStep(0); setPlaying(true); }
            else setPlaying((p) => !p);
          }}
          aria-label={playing && !done ? "Pause" : "Play"}
        >
          {playing && !done ? "Pause" : done ? "Replay" : "Play"}
        </button>
        <button className="wt-btn" onClick={() => setStep((s) => Math.min(trace.length, s + 1))} disabled={done} aria-label="Step forward">▸</button>
        <input
          className="wt-scrub"
          type="range"
          min="0"
          max={trace.length}
          value={step}
          onChange={(e) => { setPlaying(false); setStep(Number(e.target.value)); }}
        />
        <span className="wt-count">{step} / {trace.length}</span>
        <select className="wt-speed" value={speed} onChange={(e) => setSpeed(e.target.value)}>
          <option value="slow">Slow</option>
          <option value="normal">Normal</option>
          <option value="fast">Fast</option>
        </select>
      </div>

      <div className="wt-tokens">
        {tokens.map((t, i) => {
          const isCurrent = i === currentPos;
          const style = isCurrent ? undefined : { color: CATEGORY_COLOR_VAR[tokenCategory(t.type)] };
          return (
            <span key={i} className={`wt-tok${isCurrent ? " current" : ""}`} style={style}>
              {t.lexeme || (t.type === "EOF" ? "␣" : "")}
            </span>
          );
        })}
      </div>

      <div className="wt-body">
        <div className="wt-tree-col">
          <TreeZoomControls scale={scale} onZoomIn={zoomIn} onZoomOut={zoomOut} onFit={fit} />
          <div className="tree-scroll wt-tree" ref={containerRef}>
            <TreeSvg layout={layout} revealed={revealed} justRevealedId={justRevealedId} scale={scale} />
          </div>
        </div>
        <div className="wt-log" ref={logRef}>
          {trace.slice(0, step).map((ev, i) => {
            const d = describeEvent(ev);
            return <div key={i} className={`wt-log-line ${d.tone}`}>{d.text}</div>;
          })}
          {step === 0 && <div className="wt-log-line hint">Press Play or step forward to begin.</div>}
        </div>
      </div>
    </div>
  );
}
