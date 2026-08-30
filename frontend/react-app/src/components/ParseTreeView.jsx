import { useMemo } from "react";
import { layoutTree } from "../lib/treeLayout.js";
import { tokenCategory, CATEGORY_COLOR_VAR } from "../lib/tokenStyle.js";
import { useTreeZoom } from "../hooks/useTreeZoom.js";
import TreeZoomControls from "./TreeZoomControls.jsx";

function TreeSvg({ layout, scale }) {
  const { nodes, edges, width, height } = layout;
  return (
    <svg
      viewBox={`0 0 ${width} ${height}`}
      width={width * scale}
      height={height * scale}
      role="img"
      aria-label={`Parse tree with ${nodes.length} nodes`}
    >
      {edges.map(([p, c], i) => {
        const midY = (p._y + c._y) / 2;
        return (
          <path
            key={i}
            d={`M ${p._x} ${p._y} V ${midY} H ${c._x} V ${c._y}`}
            fill="none"
            stroke="var(--border)"
            strokeWidth="1.2"
          />
        );
      })}
      {nodes.map((n, i) => {
        const w = Math.max(28, n.label.length * 6.6 + 16);
        const h = 22;
        let fill = "var(--panel-2)";
        let stroke = "var(--border)";
        let tcolor = "var(--text-dim)";
        let fam = "IBM Plex Sans";
        if (n.token) {
          const color = CATEGORY_COLOR_VAR[tokenCategory(n.token.type)];
          fill = "var(--panel)";
          stroke = color;
          tcolor = color;
          fam = "IBM Plex Mono";
        } else if (n._leaf) {
          fill = "var(--panel)";
          stroke = "var(--border)";
          tcolor = "var(--text-faint)";
          fam = "IBM Plex Mono";
        }
        if (n.isError) {
          fill = "var(--error-soft)";
          stroke = "var(--error)";
          tcolor = "var(--error)";
        }
        return (
          <g key={i}>
            <rect
              x={n._x - w / 2}
              y={n._y - h / 2}
              width={w}
              height={h}
              rx="3"
              fill={fill}
              stroke={stroke}
              strokeWidth="1.2"
            />
            <text x={n._x} y={n._y + 4} textAnchor="middle" fontSize="11" fontFamily={fam} fill={tcolor}>
              {n.label}
            </text>
          </g>
        );
      })}
    </svg>
  );
}

export default function ParseTreeView({ tree }) {
  const layout = useMemo(() => layoutTree(tree), [tree]);
  const { containerRef, scale, fit, zoomIn, zoomOut } = useTreeZoom(layout.width, layout.height);

  return (
    <div>
      <div className="legend">
        <div className="legend-item">
          <span className="legend-swatch" style={{ background: "var(--panel-2)", border: "1px solid var(--border)" }} />
          non-terminal
        </div>
        <div className="legend-item">
          <span className="legend-swatch" style={{ background: "var(--panel)", border: "1px solid var(--syn-keyword)" }} />
          keyword
        </div>
        <div className="legend-item">
          <span className="legend-swatch" style={{ background: "var(--panel)", border: "1px solid var(--syn-ident)" }} />
          identifier
        </div>
        <div className="legend-item">
          <span className="legend-swatch" style={{ background: "var(--panel)", border: "1px solid var(--syn-num)" }} />
          number
        </div>
        <div className="legend-item">
          <span className="legend-swatch" style={{ background: "var(--error-soft)", border: "1px solid var(--error)" }} />
          recovered error
        </div>
        <TreeZoomControls scale={scale} onZoomIn={zoomIn} onZoomOut={zoomOut} onFit={fit} />
      </div>
      <div className="tree-scroll tree-scroll-lg" ref={containerRef}>
        <TreeSvg layout={layout} scale={scale} />
      </div>
    </div>
  );
}
