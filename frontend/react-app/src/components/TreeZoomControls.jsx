export default function TreeZoomControls({ scale, onZoomIn, onZoomOut, onFit }) {
  return (
    <div className="zoom-controls">
      <button className="wt-btn" onClick={onZoomOut} aria-label="Zoom out">−</button>
      <button className="wt-btn" onClick={onFit} title="Fit whole tree in view" aria-label="Fit to view">
        Fit
      </button>
      <button className="wt-btn" onClick={onZoomIn} aria-label="Zoom in">+</button>
      <span className="zoom-pct">{Math.round(scale * 100)}%</span>
    </div>
  );
}
